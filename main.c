#include "common.h"
#include "crc64.h"
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#define Number_Outstanding 10
#define MSG_SIZE  8
struct ibv_ah *ah;
int32_t start_counting=0;
int32_t execution_status=1;
int32_t testtime=50;
int32_t warmup=25;

static __inline__ unsigned long long get_cycle_count(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


void start_count(int sig)
{
  static int iter = 1;
  if (iter){
	printf("1\n");
	fflush(stdout);
	start_counting = 1;
	iter=0;
    alarm(testtime-warmup);
  }
  else{
	execution_status = 0; 
	printf("0\n");
	fflush(stdout);
  }
}


	
struct ibv_send_wr *bad_send_wr;
struct ibv_wc wc[Q_DEPTH];

static struct ctrl_blk *init_ctx(struct ctrl_blk *ctx, 
	struct ibv_device *ib_dev)
{
	ctx->context = ibv_open_device(ib_dev);
	CPE(!ctx->context, "Couldn't get context", 0);

	ctx->pd = ibv_alloc_pd(ctx->context);
	CPE(!ctx->pd, "Couldn't allocate PD", 0);

	create_qp(ctx);
	modify_qp_to_init(ctx);

	return ctx;
}

void post_recvs(struct ctrl_blk *cb, int num_recvs, int qpn,
	volatile int64_t  *local_addr, int local_key, int size)
{
	int ret;
	struct ibv_sge list = {
		.addr	= (uintptr_t) local_addr,
		.length = size,
		.lkey	= local_key,
	};
	struct ibv_recv_wr wr = {
		.sg_list    = &list,
		.num_sge    = 1,
	};
	struct ibv_recv_wr *bad_wr;
	int i;
	for (i = 0; i < num_recvs; i++) {
		ret = ibv_post_recv(cb->qp[qpn], &wr, &bad_wr);
		CPE(ret, "Error posting recv\n", ret);
	}
}



void post_send(struct ctrl_blk *cb, int qpn, 
	volatile int64_t *local_addr, int local_key, int signal, int size)
{ 
	
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;	
	cb->wr.opcode = IBV_WR_SEND;


	if(signal) {
		cb->wr.send_flags |= IBV_SEND_SIGNALED;
	}

	cb->wr.send_flags |= IBV_SEND_INLINE;	
 
	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

	int ret = ibv_post_send(cb->qp[qpn], &cb->wr, &bad_send_wr);
	
	CPE(ret, "ibv_post_send error", ret);
}

void post_read(struct ctrl_blk *cb, int qpn, 
	volatile int64_t *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int signal,int size)
{
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
		
	cb->wr.opcode = IBV_WR_RDMA_READ;
//	cb->wr.send_flags = 0; for batching it is required
	if(signal)
	cb->wr.send_flags |= IBV_SEND_SIGNALED;
	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;
	cb->wr.wr.rdma.remote_addr = remote_addr;
	cb->wr.wr.rdma.rkey = remote_key;
/*
	struct ibv_send_wr wrbatch[Number_Batching];
	int i;
	for(i=0;i<Number_Batching;i++){
	wrbatch[i]=cb->wr;
	if(i==(Number_Batching-1))
	wrbatch[i].next=NULL;
    else		
	 wrbatch[i].next=&wrbatch[i+1];
   }	
*/			
	int ret = ibv_post_send(cb->qp[qpn], &cb->wr , &bad_send_wr); //&wrbatch[0]
		
	CPE(ret, "ibv_post_send error", ret);

}

void post_write(struct ctrl_blk *cb, int qpn, 
	volatile int64_t *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int signal, int size)
{
//	int number_Batching;
//	number_Batching=Number_Batching;
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
	
	cb->wr.opcode = IBV_WR_RDMA_WRITE;
	
    
//    cb->wr.send_flags=0;  //it is must be for batching 
	if(signal){
		cb->wr.send_flags |= IBV_SEND_SIGNALED;
	}
//	cb->wr.send_flags |= IBV_SEND_INLINE; 

 
	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

	cb->wr.wr.rdma.remote_addr = remote_addr;
	cb->wr.wr.rdma.rkey = remote_key;	
	
/*	struct ibv_send_wr wrbatch[number_Batching];
	int i;
	
	for(i=0;i<number_Batching;i++){
	wrbatch[i]=cb->wr;
	if(i==(number_Batching-1))
	wrbatch[i].next=NULL;
    else		
	 wrbatch[i].next=&wrbatch[i+1];
   }
*/	
	int ret = ibv_post_send(cb->qp[qpn], &cb->wr, &bad_send_wr); //  &wrbatch[0]
    if(ret){
    	perror("WRITE ERROR:");
    }

	CPE(ret, "ibv_post_send error", ret);
}


void post_atomic(struct ctrl_blk *cb, int qpn, 
	volatile int64_t *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int64_t compare, int64_t swap, int signal, int size)
{
//	int number_Batching;
//	number_Batching=Number_Batching;
	cb->sgl.addr = (int64_t) (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
	
	cb->wr.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;  //          IBV_WR_ATOMIC_FETCH_AND_ADD
	
    
//    cb->wr.send_flags=0;  //it is must be for batching 
	if(signal){
		cb->wr.send_flags |= IBV_SEND_SIGNALED;
	}
//	cb->wr.send_flags |= IBV_SEND_INLINE; 
 
	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

        cb->wr.wr.atomic.remote_addr = remote_addr;
        cb->wr.wr.atomic.rkey =remote_key ;
        cb->wr.wr.atomic.compare_add = compare ; //1ULL;
        cb->wr.wr.atomic.swap = swap ;

//	cb->wr.wr.rdma.remote_addr = remote_addr;
//	cb->wr.wr.rdma.rkey = remote_key;	
	
/*	struct ibv_send_wr wrbatch[number_Batching];
	int i;
	
	for(i=0;i<number_Batching;i++){
	wrbatch[i]=cb->wr;
	if(i==(number_Batching-1))
	wrbatch[i].next=NULL;
    else		
	 wrbatch[i].next=&wrbatch[i+1];
   }
*/	
	int ret = ibv_post_send(cb->qp[qpn], &cb->wr, &bad_send_wr); //  &wrbatch[0]

	CPE(ret, "ibv_post_send error", ret);

}




void run_server(struct ctrl_blk *cb)
{
 
    int client_id=0;
//  int sent[NUM_CLIENTS];
//  memset(sent, 0, NUM_CLIENTS * sizeof(int));
  //memset( (uint64_t *)server_resp_area, 0, NUM_CLIENTS * sizeof(int64_t)); //  requires for  READ-READ and WRITE-READ
//	struct ibv_wc *wc = (struct ibv_wc *) malloc(sizeof(struct ibv_wc)); // requires for SEND-SEND

     	while(1) { //execution_status


// SEND

            post_recvs(cb, S_DEPTH, client_id, &server_req_area[client_id], server_req_area_mr->lkey, MSG_SIZE);     		
			poll_cq(cb->cq[client_id], S_DEPTH, cb->id);											
      		client_id = (client_id+1) % NUM_CLIENTS; 

						
/* WRITE-WRITE
     		if (server_req_area[client_id]==10) {
					server_req_area[client_id]=20;		            
					post_write(cb, client_id, 
							&server_req_area[client_id], server_req_area_mr->lkey, 
							client_resp_area_stag[client_id].buf, client_resp_area_stag[client_id].rkey, 1,
							sizeof(uint64_t));
					poll_cq(cb->cq[client_id], 1,cb->id);											
			} 
			client_id=(client_id+1)%NUM_CLIENTS; 
*/

/* READ-WRITE
               *server_local_read = 20;
    			post_read(cb, client_id, 
				server_local_read, server_local_read_mr->lkey, 
				client_req_area_stag[client_id].buf, client_req_area_stag[client_id].rkey,1, 
				sizeof(uint64_t)); 
				poll_cq(cb->cq[client_id], 1, cb->id);	

             if (*server_local_read == 10){               	
                * server_resp_area = 20;                	                	                          	                	               	
	            post_write(cb, client_id, 
				server_resp_area, server_resp_area_mr->lkey, 
				client_resp_area_stag[client_id+1].buf, client_resp_area_stag[client_id+1].rkey, 1, sizeof(uint64_t));
				poll_cq(cb->cq[client_id+1], 1, cb->id);	      
				}

			    client_id=(client_id+1)%NUM_CLIENTS; 	
*/

/* WRITE-READ

            
			if( server_req_area[client_id] == 10){
				__sync_val_compare_and_swap (&server_req_area[client_id], 10, 0);
				__sync_val_compare_and_swap (&server_resp_area[client_id], 0, 20);			
		    }
            client_id = (client_id+1) % NUM_CLIENTS; 
	
*/


/* READ-READ
    			post_read(cb, client_id, 
				server_local_read, server_local_read_mr->lkey, 
				client_req_area_stag[client_id].buf, client_req_area_stag[client_id].rkey,1, 
				sizeof(uint64_t)); 				
				poll_cq(cb->cq[client_id], 1, cb->id);	
                if (* server_local_read == 10){   
                   //server_resp_area[client_id]=20;
                	__sync_val_compare_and_swap (&server_resp_area[client_id], 0, 20);
                }	
                client_id = (client_id+1) % NUM_CLIENTS; 
*/


/* SOCKET-SOCKET

				int sockfd, newsockfd[NUM_CLIENTS], i;
				struct sockaddr_in serv_addr;
                //int64_t read=0, write=0;
				sockfd = socket(AF_INET, SOCK_STREAM, 0);
				if (sockfd < 0) {
					perror("ERROR opening socket:\n");
					exit(1);
				}


				int on = 1, status = -1;
			    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
			        		(const char *) &on, sizeof(on));
			    if (-1 == status) {
			        perror("setsockopt(...,SO_REUSEADDR,...)");
			        exit(1);
			    }

				bzero((char *) &serv_addr, sizeof(serv_addr));
				serv_addr.sin_family = AF_INET;
				serv_addr.sin_addr.s_addr = inet_addr("192.168.6.3"); //    INADDR_ANY
				serv_addr.sin_port = htons(3000);

				if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
					perror("ERROR on binding:\n");
				}else{
					printf("Bounded Successfully\n");
					fflush(stdout);
				}




				printf("Server %d listening on port %d\n", cb->id, 3000);
				listen(sockfd, NUM_CLIENTS);

				i=0;
				while(i < NUM_CLIENTS){               
					printf("Server %d trying to accept(%d)\n", cb->id, i);
					newsockfd[i] = accept(sockfd, NULL, NULL);
					if (newsockfd[i] < 0) {
						perror("ERROR on accept: \n");
					}else {
					printf("Connection %d connected \n", i); 
					fflush(stdout);
					fcntl(newsockfd[i], F_SETFL, O_NONBLOCK); // Change the socket into non-blocking state	
				    i++;
				    }
				}


			    char read[18];
	            char write[18];

                while(1){                	    
			   	        recv(newsockfd[client_id], read, 8, 0);				             				            
			   	        if(strncmp(read,"abcdefgh",8)==0){			   	        	
			                strcpy (write,"abcdefgh");
			                strcpy(read,"obcdefgh");			                
				            if(send(newsockfd[client_id], write, 8,0) < 0)
						        perror("ERROR sending from socket: \n");	 
			   	        }
   	        			client_id=(client_id+1)%NUM_CLIENTS; 
		   	    }

*/

/* SEND-SEND

	   	    
		   	  if(!sent[client_id]) {
   		          post_recvs(cb, 1, client_id, &server_req_area[client_id], server_req_area_mr->lkey, sizeof(uint64_t));	
   		          sent[client_id] = 1;
   		      }           	  
   		      if(ibv_poll_cq(cb->cq[client_id], 1, wc )>0){
   		          sent[client_id] = 0; 		     
   		          //if(server_req_area[client_id]==10){      		          	
	   		        //server_resp_area[client_id]= 20;
	   		        //server_req_area[client_id]= 0;
	                post_send(cb, client_id, &server_resp_area[client_id], server_resp_area_mr->lkey, 1, sizeof(uint64_t));
	                poll_cq(cb->cq[client_id], 1, 1);			                   
                  //}                                
              }
              client_id = (client_id+1) % NUM_CLIENTS;   
*/





        }
}

void run_client(struct ctrl_blk *cb)
{

    int32_t * random_key =  (int32_t * ) malloc(4000000 * sizeof(int32_t));

	
	
	  FILE *fp;
      int i,counter=0;
   
      /* open the file  for different distributions  {Zipf,uniform}
		zipf_random_numbers.txt
		uniform_random_numbers.txt
	  */   
     
   fp = fopen("zip_random_numbers.txt", "r"); 
      if (fp == NULL) {
         printf("I couldn't open distribution file for reading.\n");
         exit(0);
      }

  
  printf("Loading distribution to test\n");
  fflush(stdout); 
     srand(time(NULL));   // should only be called once  
      while (fscanf(fp, "%d,", &i) == 1){  
		  random_key[counter] = i;				
		  counter++;	  
	  }
		
  printf("Loading %d is finished ready to Connect\n", counter );
  fflush(stdout);
	

	int64_t num_req = 0;
	
	int sn = cb->id/NUM_CLIENTS_PER_SERVER; 


/* SOCKET-SOCKET
			    int sockfd,  sock_port;
				struct sockaddr_in serv_addr;
				struct hostent *server;

				sock_port = 3000;

				sockfd = socket(AF_INET, SOCK_STREAM, 0);
			
				server = gethostbyname("192.168.6.3");
				CPE(server == NULL, "No such host", 0);
			
				bzero((char *) &serv_addr, sizeof(serv_addr));
				serv_addr.sin_family = AF_INET;
				bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
					server->h_length);
				serv_addr.sin_port = htons(sock_port);

				while(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
					perror("ERROR connecting \n");
				}
					
*/


	int iteration=0;
	
	* client_req_area = 0;
	
	while(execution_status){


			if (start_counting){
		        iteration=iteration+Number_Batching;	
           	}
				num_req++;
	

//Code for posting a new request

                                                         /* <<<<<<<<<<<Primary operations>>>>>>>> */
// SEND
      if(num_req%S_DEPTH){
	       post_send(cb, sn, client_req_area, client_req_area_mr->lkey, 0, MSG_SIZE);            		       
      }else{
           post_send(cb, sn, client_req_area, client_req_area_mr->lkey, 1, MSG_SIZE);            	
	       poll_cq(cb->cq[sn], S_DEPTH, cb->id);	
	       num_req=0;
      }


/* WRITE
            
      if(num_req%S_DEPTH){
            post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_req_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_req_area_stag[sn].rkey,  
				0, sizeof(uint64_t));
	  }else{
            post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_req_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_req_area_stag[sn].rkey,  
				1, sizeof(uint64_t));
		    poll_cq(cb->cq[sn], S_DEPTH, cb->id);
		    num_req=0;
      }
 */    
 
/* READ


      if(num_req%S_DEPTH){
            post_read(cb, sn, client_local_read, client_local_read_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,0, 
				sizeof(uint64_t)); 
	  }else{
            post_read(cb, sn, client_local_read, client_local_read_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,1, 
				sizeof(uint64_t)); 
	        poll_cq(cb->cq[sn], S_DEPTH, cb->id);
		    num_req=0;
      }

*/	

/*Atomic CAS and FAA
   			        post_atomic(cb, sn, 
					client_req_area, client_req_area_mr->lkey, 
					server_req_area_stag[sn].buf, server_req_area_stag[sn].rkey,  //+(cb->id*8)
					1,8);	
			        poll_cq(cb->cq[sn], 1);
*/


/* SOCKET

        char  write[8];
        strcpy (write,"abcdefgh");		
	    if(send(sockfd, write, 8, 0) < 0)
			fprintf(stderr, "ERROR reading from socket");					
*/

                                                    /* <<<<<<<<<<<<<<Optimizations>>>>>>>>>>>> */

/*Outstanding   int i;
				if(np){
				struct ibv_wc *wc = (struct ibv_wc *) malloc( 2 * Number_Outstanding * sizeof(struct ibv_wc));		
	    	    num_req=ibv_poll_cq(cb->cq[sn], Number_Outstanding, wc);
				}
                for(i=0;i<num_req;i++){
				post_write(cb, sn, 
					client_req_area, client_req_area_mr->lkey, 
					server_req_area_stag[sn].buf+(cb->id*FARM_ITEM_SIZE), server_req_area_stag[sn].rkey,  //
					1,FARM_ITEM_SIZE);	
					//usleep(1000);
				}	
						
				np=1;
*/				
					  			  
                 
/*Batching		memset((char *) client_resp_area, 'b' , FARM_ITEM_SIZE  ); 
				num_req++;
//				if(num_req%(S_DEPTH)){
				    post_read(cb, sn, 
					client_resp_area, client_resp_area_mr->lkey,  // rand_key[random_key[num_req%4000000]] * 2 random_key[num_req%4000000] 
					server_req_area_stag[sn].buf , server_req_area_stag[sn].rkey,1, //random_key[num_req%4000000]
					FARM_ITEM_SIZE);  
//					printf("here:%lu\n",sizeof(struct request_s));
//					fflush(stdout);
//					exit(1);
//				}
//				else{
//				    post_read(cb, sn, 
//					client_resp_area, client_resp_area_mr->lkey, 
//					server_req_area_stag[sn].buf+random_key[num_req%4000000], server_req_area_stag[sn].rkey,1, //random_key[num_req%4000000]
//					FARM_ITEM_SIZE); 
//				struct ibv_wc *wc = (struct ibv_wc *) malloc( 2 * Number_Batching * sizeof(struct ibv_wc));		
//	    	    num_req=ibv_poll_cq(cb->cq[sn], Number_Batching, wc);
				poll_cq(cb->cq[sn], 1);
//				while(client_resp_area[0]!='a'){}
//					num_req=0;
				
*/
			

/*Unsignalled 
			 if(num_req%S_DEPTH)
				post_write(cb, sn, 
					client_req_area, client_req_area_mr->lkey, 
					server_req_area_stag[sn].buf+(cb->id*FARM_ITEM_SIZE), server_req_area_stag[sn].rkey,  //
					0,FARM_ITEM_SIZE);
			else{
				post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_req_area_stag[sn].buf+(cb->id*FARM_ITEM_SIZE), server_req_area_stag[sn].rkey,  //
				1,FARM_ITEM_SIZE);
				poll_cq(cb->cq[sn], 1);
				num_req=0;

			}	
*/
                                                  /* <<<<<<<<<<Communicaion paradigms>>>>>>>> */

/* WRITE-WRITE
                *client_req_area=10;
                *client_resp_area=10;
				post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_req_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_req_area_stag[sn].rkey,  
				1, sizeof(uint64_t));
				poll_cq(cb->cq[sn], 1,cb->id);				
				while(*client_resp_area!=20){									
				}
*/				

                                                   

/* READ-WRITE

                 __sync_val_compare_and_swap (client_req_area, 0, 10);
			    // * client_req_area=10;
				while(*client_resp_area!=20){}
		        __sync_val_compare_and_swap (client_req_area, 10, 0);
			    // * client_req_area=0;// before become zero server read and go to write to the client_resp
				__sync_val_compare_and_swap (client_resp_area, 20, 10);				
				// * client_resp_area=10;
*/



/* WRITE-READ

                * client_req_area = 10;
                post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_req_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_req_area_stag[sn].rkey,  //
				1, sizeof(uint64_t));
				poll_cq(cb->cq[sn], 1, cb->id);	

again:			post_read(cb, sn, 
				client_local_read, client_local_read_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,1, 
				sizeof(uint64_t)); 
				poll_cq(cb->cq[sn], 1, cb->id);				

				if(* client_local_read != 20)
					goto again;

                * client_resp_area = 0;
                post_write(cb, sn, 
				client_resp_area, client_resp_area_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,  //
				1, sizeof(uint64_t));
				poll_cq(cb->cq[sn], 1, cb->id);	
*/



/* READ-READ

                * client_req_area=10;


again:			post_read(cb, sn, 
				client_local_read, client_local_read_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,1, 
				sizeof(uint64_t)); 
				poll_cq(cb->cq[sn], 1, cb->id);	

				if(* client_local_read != 20)
					goto again;

				* client_req_area=0;
				post_write(cb, sn, 
				client_req_area, client_req_area_mr->lkey, 
				server_resp_area_stag[sn].buf + (cb->id * sizeof(uint64_t)), server_resp_area_stag[sn].rkey,  //
				1, sizeof(uint64_t));
				poll_cq(cb->cq[sn], 1, cb->id);	
            
*/



/*  SOCKET-SOCKET


                char  write[8];
                char read[8];
                strcpy (write,"abcdefgh");
		   	    strcpy(read,"obcdefgh");

	            if(send(sockfd, write, 8, 0) < 0)
			        fprintf(stderr, "ERROR reading from socket");	
rec:		   	if(recv(sockfd, read, 8, 0) < 0) 
				    fprintf(stderr, "ERROR reading from socket");			   	
			   	if(strcmp(read,"abcdefgh")!=0)
			   		goto rec;

*/

/* SEND-SEND

              * client_req_area = 10;
              post_send(cb, sn, client_req_area, client_req_area_mr->lkey, 1, sizeof(uint64_t));     
              //poll_cq(cb->cq[sn], 1, cb->id);
  		      post_recvs(cb, 1, sn, client_resp_area, client_resp_area_mr->lkey, sizeof(uint64_t));	  	   		      
              poll_cq(cb->cq[sn], 2, cb->id);
		   	  if(*client_resp_area==20) {
   		          *client_resp_area=10;
   		      }           	  			   	               		

*/

			

			 
	
}

	FILE *out1 = fopen("throughput.txt", "a");  
    printf("iteration:%d\n",iteration);
    fprintf(out1, "%d\n", iteration);  
    fclose(out1);
	printf("Result is printed to the file\n");
	fflush(stdout);	

	
	return;
}


int main(int argc, char *argv[])
{
	
	int i;
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
	struct ctrl_blk *ctx;

	srand48(getpid() * time(NULL));		//Required for PSN
	ctx = malloc(sizeof(struct ctrl_blk));

	ctx->id = atoi(argv[1]);

	if (argc == 2) {
		ctx->is_client = 1;
		ctx->num_conns = NUM_SERVERS;

		ctx->local_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		ctx->remote_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		
	} else {
		ctx->sock_port = atoi(argv[2]);
		ctx->num_conns = NUM_CLIENTS;

		ctx->local_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
		ctx->remote_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
	}
	
	dev_list = ibv_get_device_list(NULL);
	CPE(!dev_list, "Failed to get IB devices list", 0);

	ib_dev = dev_list[is_roce() == 1 ? 1 : 0];
	//ib_dev = dev_list[0];
	CPE(!ib_dev, "IB device not found", 0);

	init_ctx(ctx, ib_dev);
	CPE(!ctx, "Init ctx failed", 0);


	setup_buffers(ctx);


	union ibv_gid my_gid= get_gid(ctx->context);

	for(i = 0; i < ctx->num_conns; i++) {
		ctx->local_qp_attrs[i].gid_global_interface_id = 
			my_gid.global.interface_id;
		ctx->local_qp_attrs[i].gid_global_subnet_prefix = 
			my_gid.global.subnet_prefix;

		ctx->local_qp_attrs[i].lid = get_local_lid(ctx->context);
		ctx->local_qp_attrs[i].qpn = ctx->qp[i]->qp_num;
		ctx->local_qp_attrs[i].psn = lrand48() & 0xffffff;
		fprintf(stderr, "Local address of RC QP %d: ", i);
		print_qp_attr(ctx->local_qp_attrs[i]);
	}

	if(ctx->is_client) {
		client_exch_dest(ctx);
	} else {
		server_exch_dest(ctx);
	}

	if (ctx->is_client) {
		for(i = 0; i < NUM_SERVERS; i++) {
			if(connect_ctx(ctx, ctx->local_qp_attrs[i].psn, 
				ctx->remote_qp_attrs[i], ctx->qp[i], 1)) {  // Unreliable Connection: 0 also modify the connect_ctx in conn.c
				return 1;
			}
		}
	}

	if(ctx->is_client) {
	   signal (SIGALRM,start_count); 
	   alarm(warmup);		
	   run_client(ctx);
	} else {		
		run_server(ctx);
	}
	return 0;
}
