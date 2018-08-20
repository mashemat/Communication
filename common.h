#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>

#include "size.h"
#ifndef IBV_PINGPONG_H
#define IBV_PINGPONG_H
#include <infiniband/verbs.h>

#define IB_PHYS_PORT 1			//Primary physical port number for qps


#define WINDOW_SIZE 1   
#define WINDOW_SIZE_ 1

#define SERVER_AREA  5000000 
#define Number_Batching 1
#define Q_DEPTH 2048
#define S_DEPTH 512/Number_Batching


#define REQ_AC 1024		//Keep it at 32KB: fits all clients!
#define RESP_AC WINDOW_SIZE

#define M_1 1048576
#define M_1_ 1048575

#define K_512 524288
#define K_512_ 524287

#define MB16_ 16777215

#define READ_1 11
#define READ_2 12
#define WRITE_1 14


#define FENCE asm volatile ("" : : : "memory"); \
	asm volatile("mfence" ::: "memory")

#define CPE(val, msg, err_code) \
	if(val) { fprintf(stderr, msg); fprintf(stderr, " Error %d \n", err_code); \
	exit(err_code);}

//Time measurements
# define START_TIME(NOTE, X) \
  do { clock_gettime(CLOCK_REALTIME,&X); \
	fprintf(stderr, "%s", NOTE); } while(0)

#define END_TIME(NOTE, X, Y) \
  do { clock_gettime(CLOCK_REALTIME, &Y); \
    printf("%s= %f\n", NOTE, (Y.tv_sec - X.tv_sec) + \
		(double)(Y.tv_nsec - X.tv_nsec) / 1000000000); } \
  while (0)

#define MEASURE_IOPS(X,Y) \
  do { clock_gettime(CLOCK_REALTIME, &Y); \
    fprintf(stdout, "%f\n", ITERS_PER_MEASUREMENT / \
		((Y.tv_sec - X.tv_sec) + \
		(double)(Y.tv_nsec - X.tv_nsec) / 1000000000)); \
	fflush(stdout);} \
  while (0)

struct qp_attr {
	uint64_t gid_global_interface_id;	// Store the gid fields separately
	uint64_t gid_global_subnet_prefix; 	// because I don't like unions
	int lid;
	int qpn;
	int psn;
};
#define S_QPA sizeof(struct qp_attr)

struct ctrl_blk {
	struct ibv_context *context;
	struct ibv_pd *pd;

	struct ibv_cq **cq;
	struct ibv_qp **qp;
	struct qp_attr *local_qp_attrs;
	struct qp_attr *remote_qp_attrs;
	
	struct ibv_ah *client_ah;
	
	struct ibv_send_wr wr;
	struct ibv_sge sgl;

	int num_conns;
	int is_client, id;
	int sock_port;
};

struct stag {
	uint64_t buf;
	uint32_t rkey;
	uint32_t size;
};



#define S_STG sizeof(struct stag)

//Keep some stuff global
volatile  int64_t * server_req_area, * server_resp_area, * server_local_read;
volatile int64_t  * client_resp_area, * client_local_read;
volatile int64_t * client_req_area;

struct ibv_mr *server_req_area_mr, *client_req_area_mr, *client_resp_area_mr, * server_resp_area_mr, * server_local_read_mr, * client_local_read_mr ;

//Only the server's request area is accessed by RDMA (i.e. client's RDMA READs)
struct stag server_req_area_stag[NUM_SERVERS], client_resp_area_stag[NUM_CLIENTS], client_req_area_stag[NUM_CLIENTS], server_resp_area_stag[NUM_SERVERS] ;

union ibv_gid get_gid(struct ibv_context *context);
uint16_t get_local_lid(struct ibv_context *context);

void create_qp(struct ctrl_blk *ctx);
void modify_qp_to_init(struct ctrl_blk *ctx);
int setup_buffers(struct ctrl_blk *cb);

void client_exch_dest(struct ctrl_blk *ctx);
void server_exch_dest(struct ctrl_blk *ctx);

int connect_ctx(struct ctrl_blk *ctx, int my_psn, struct qp_attr dest, 
	struct ibv_qp *qp, int use_uc);

int close_ctx(struct ctrl_blk *ctx);

void print_stag(struct stag);
void print_qp_attr(struct qp_attr);

void micro_sleep(double microseconds);

void set_signal(int num, struct ctrl_blk *cb);
void poll_cq(struct ibv_cq *cq, int num_completions, int id);
int is_roce(void);
inline uint32_t fastrand(uint64_t* seed);

#endif /* IBV_PINGPONG_H */
