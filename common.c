#include "common.h"

union ibv_gid get_gid(struct ibv_context *context)
{
	union ibv_gid ret_gid;
	ibv_query_gid(context, IB_PHYS_PORT, 0, &ret_gid);

	fprintf(stderr, "GID: Interface id = %lld subnet prefix = %lld\n", 
		(long long) ret_gid.global.interface_id, 
		(long long) ret_gid.global.subnet_prefix);
	
	return ret_gid;
}

uint16_t get_local_lid(struct ibv_context *context)
{
	struct ibv_port_attr attr;

	if (ibv_query_port(context, IB_PHYS_PORT, &attr))
		return 0;

	return attr.lid;
}

int close_ctx(struct ctrl_blk *ctx)
{
	int i;
	for(i = 0; i < ctx->num_conns; i++) {
		if (ibv_destroy_qp(ctx->qp[i])) {
			fprintf(stderr, "Couldn't destroy connected QP\n");
			return 1;
		}
		if (ibv_destroy_cq(ctx->cq[i])) {
			fprintf(stderr, "Couldn't destroy connected CQ\n");
			return 1;
		}
	}
	
	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	free(ctx);

	return 0;
}

void create_qp(struct ctrl_blk *ctx)
{
	int i;
	//Create connected queue pairs
	ctx->qp = malloc(sizeof(int *) * ctx->num_conns);
	ctx->cq = malloc(sizeof(int *) * ctx->num_conns);

	for(i = 0; i < ctx->num_conns; i++) {
		ctx->cq[i] = ibv_create_cq(ctx->context, 
			Q_DEPTH + 1, NULL, NULL, 0);
		CPE(!ctx->cq[i], "Couldn't create CQ", 0);

		struct ibv_qp_init_attr init_attr = {
			.send_cq = ctx->cq[i],
			.recv_cq = ctx->cq[i],
			.cap     = {
				.max_send_wr  = Q_DEPTH,
				.max_recv_wr  = Q_DEPTH,
				.max_send_sge = 1,
				.max_recv_sge = 1,
				.max_inline_data = 800
			},
			.qp_type = IBV_QPT_UC  // IBV_QPT_UC 
		};
		ctx->qp[i] = ibv_create_qp(ctx->pd, &init_attr);
		CPE(!ctx->qp[i], "Couldn't create connected QP", 0);
	}  
}

void modify_qp_to_init(struct ctrl_blk *ctx)
{
	
	

	int i;

	struct ibv_qp_attr conn_attr = {
		.qp_state		= IBV_QPS_INIT,
		.pkey_index		= 0,
		.port_num		= IB_PHYS_PORT,
		.qp_access_flags= IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC /	};

	int init_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;


	for(i = 0; i < ctx->num_conns; i++) {
		if (ibv_modify_qp(ctx->qp[i], &conn_attr, init_flags)) {
			fprintf(stderr, "Failed to modify QP to INIT \n");
			return;
		}
	}
		
}

int setup_buffers(struct ctrl_blk *cb)
{

	int FLAGS = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
				IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
	if (cb->is_client) {

		client_resp_area = memalign(4096, SERVER_AREA * sizeof(int64_t)); // FARM_INLINE_READ_SIZE * WINDOW_SIZE
		client_resp_area_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) client_resp_area , SERVER_AREA * sizeof(int64_t) , FLAGS); // FARM_INLINE_READ_SIZE * WINDOW_SIZE
		 memset((int64_t *) client_resp_area, 0 , SERVER_AREA * sizeof(int64_t) ); // 



        client_local_read =memalign(4096,  sizeof(int64_t)); 
        * client_local_read= 0;
		client_local_read_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) client_local_read, sizeof(int64_t), FLAGS); 


		// For WRITE-ing to server
		client_req_area = memalign(4096, SERVER_AREA * sizeof(int64_t) ); // 
		memset((int64_t *) client_req_area, 0 , SERVER_AREA * sizeof(int64_t) ); // 
		client_req_area_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) client_req_area , SERVER_AREA * sizeof(int64_t), FLAGS); //   

	} else {

        server_resp_area =memalign(4096,  SERVER_AREA * sizeof(int64_t)); 
        * server_resp_area= 20;
		server_resp_area_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) server_resp_area,  SERVER_AREA * sizeof(int64_t) , FLAGS); 


        server_local_read =memalign(4096,  sizeof(int64_t)); 
        * server_local_read= 0;
		server_local_read_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) server_local_read, sizeof(int64_t), FLAGS); 


		server_req_area = memalign(4096,  SERVER_AREA * sizeof(int64_t) ); 
        * server_req_area= 20;

		server_req_area_mr = ibv_reg_mr(cb->pd, 
			(int64_t *) server_req_area,  SERVER_AREA * sizeof(int64_t) , FLAGS); 
	}
	return 0;
}

void print_stag(struct stag st)
{
	fflush(stdout);
	fprintf(stderr, "\t%lu, %u, size===================%u\n", st.buf, st.rkey, st.size); 
}

void print_qp_attr(struct qp_attr dest)
{
	fflush(stdout);
	fprintf(stderr, "\t%d %d %d\n", dest.lid, dest.qpn, dest.psn);
}

void micro_sleep(double microseconds)
{
	struct timespec start, end;
	double sleep_seconds = (double) microseconds / 1500000;
	clock_gettime(CLOCK_REALTIME, &start);
	while(1) {
		clock_gettime(CLOCK_REALTIME, &end);
		double seconds = (end.tv_sec - start.tv_sec) + 
			(double) (end.tv_nsec - start.tv_nsec) / 1000000000;
		if(seconds > sleep_seconds) {
			return;
		}
	}
}

// Only some writes need to be signalled
void set_signal(int num, struct ctrl_blk *cb)
{	
	if(num % S_DEPTH == 0) {
		cb->wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
	} else {
		cb->wr.send_flags = IBV_SEND_INLINE;
	}
}

//Poll one of many recv CQs
void poll_cq(struct ibv_cq *cq, int num_completions, int id)
{
	
	struct timespec end;
	struct timespec start;	
	struct ibv_wc *wc = (struct ibv_wc *) malloc(
		num_completions * sizeof(struct ibv_wc));
	int comps= 0, i = 0;
	clock_gettime(CLOCK_REALTIME, &start);
	while(comps < num_completions) {		
		int new_comps = ibv_poll_cq(cq, num_completions - comps, wc);
		if(new_comps != 0) {
			comps += new_comps;
			for(i = 0; i < new_comps; i++) {
				if(wc[i].status < 0) {
					perror("poll_recv_cq error");
					exit(0);
				}
			}
		} 
		clock_gettime(CLOCK_REALTIME, &end);
		double seconds = (end.tv_sec - start.tv_sec) +
				(double) (end.tv_nsec - start.tv_nsec) / 1000000000;
		if (seconds>1){printf("Passed:%d:%d",id,comps); fflush(stdout); break;}
	}
}

int is_roce(void)
{
	char *env = getenv("ROCE");
	if(env == NULL) {		// If ROCE environment var is not set
		fprintf(stderr, "ROCE not set\n");
		exit(-1);
	}
	return atoi(env);
}

inline uint32_t fastrand(uint64_t* seed)
{
    *seed = *seed * 1103515245 + 12345;
    return (uint32_t)(*seed >> 32);
}
