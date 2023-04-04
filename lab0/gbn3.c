//GBN 2023.4.1

#include <stdio.h>
#include <string.h>

#define BIDIRECTIONAL 0   

//调试用宏定义,更好的版本
#define DEBUG 1
#define OUTPUT 0
#define P_WARNNING(str) if(DEBUG)printf("\033[0m\033[1;33m[WARNNING]\033[0m%s \n",str);if(OUTPUT)printf("%s",str)
#define P_ERROR(str) if(DEBUG)printf("\033[0m\033[1;31m[ERROR]\033[0m%s \n",str);if(OUTPUT)printf("%s",str)
#define P_OK(str) if(DEBUG)printf("\033[0m\033[1;32m[OK]\033[0m%s \n",str);if(OUTPUT)printf("%s",str)
#define P_MESSAGE(str) if(DEBUG)printf("\033[0m\033[1;34m[MESSAGE]\033[0m%s \n",str);if(OUTPUT)printf("%s",str)

//全局设置
#define OK 1
#define ERROR 0
#define UNDEFINE -1
#define WAITING 2
#define READY 3

#define A_SEND 0
#define B_SEND 1

#define OVER_TIME 100.0
#define WINDOW_SIZE 10

typedef struct msg;
typedef struct pkt;
typedef struct Window_item;
int nsimmax = 0;
int Breceive = 0;


//全局变量

struct Window_item* list;//全局消息队列
int ListCount=0;//全局总消息指针
int header=0;//窗口头指针
int tail=0;//窗口末指针

//窗口列表
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
};

struct Window_item 
{
  struct pkt Pkt;
  int state;
};

struct msg {
  char data[20];
};





//全局函数
//计算校验和
int generateChecksum(struct pkt packet)
{
  int ret = 0;
  for(int i=0;i<20;i++)
  {
    ret +=(int)(packet.payload[i]);
  }
  ret += packet.acknum;
  ret += packet.seqnum;
  return ret;
}

//封装函数：打包pkt
struct pkt generatePkg(struct msg message,int acknum,int seqnum)
{
  struct pkt packet;
  strncpy(packet.payload,message.data,20);
  packet.acknum = acknum;
  packet.seqnum = seqnum;
  packet.checksum = generateChecksum(packet);
  return packet;
}

//封装函数：获取窗口内消息总数
int windows_num(){
  return header - tail +1;
}


/********* 上层函数实现部分 *********/
checkList()
{
  P_MESSAGE("Check list\n");

  //检测是否有消息需要发出,有则发出
  while(list[header].state != UNDEFINE )
  {
      //小于窗口容量,到底则注意溢出
      if(windows_num() < WINDOW_SIZE)
      {
        if(DEBUG)printf("\n\033[0m\033[1;34m[SEND] %d/%d [header,tail] %d,%d \033[0m \n",list[header].Pkt.seqnum,ListCount,header,tail);if(OUTPUT)printf("%d",ListCount);
        tolayer3(A_SEND,list[header].Pkt);
        printf("A send: %d\n",header);

        if( header == tail)
        {
          stoptimer(A_SEND);
          starttimer(A_SEND,OVER_TIME);
          P_MESSAGE("A timer start");
        }


        if( header < ListCount-1)
        {
          header++;
        }
        if(header == nsimmax-1)
        {
          P_WARNNING("header == ListCount-1");
          for(int i=tail;i<header;i++)
          {
            printf("A send:%d\n",i);
            tolayer3(A_SEND,list[i].Pkt);
          }
        }
        else
        {
          break;
        }
      }
      
      else
      {
        P_WARNNING("List Full");
        printf("window_num:%d,lc:%d\n",windows_num(),ListCount);getchar();
        for(int i=tail;i<header;i++)
        {
          printf("A send:%d\n",i);
          tolayer3(A_SEND,list[i].Pkt);
        }
        break;
      }
  }
}


/********* 中层函数实现部分 *********/

A_output(message)
  struct msg message;
{
  //把消息打包放入全局队列
  struct Window_item* item = &list[ListCount];
  item->Pkt = generatePkg(message,UNDEFINE,ListCount++);
  item->state = READY;
  P_MESSAGE("A_output\n");
  checkList();
}


/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
  struct pkt packet;
{
  stoptimer(A_SEND);

  if (packet.checksum == generateChecksum(packet) && packet.acknum == OK)
  {
    printf("A receive: seq:%d,tail:%d\n",packet.seqnum,tail);//getchar();
    P_OK("A confim OK");
    tail = packet.seqnum;
    stoptimer(A_SEND);
    starttimer(A_SEND,OVER_TIME);
  }
  checkList();
}

//计时器结束，重发尾部消息。（乱序到达？）
A_timerinterrupt()
{
  P_WARNNING("A timer interrupt");
  header = tail;
  checkList();
}  

A_init()
{
  //全局消息队列
  list = malloc(sizeof(struct Window_item ) * nsimmax);
  for(int i=0;i<nsimmax;i++)
  {
    list[i].state = UNDEFINE;
  }

  //指针初始化
  header = 0;
  tail = 0;
  ListCount = 0;
}

/* called from layer 3, when a packet arrives for layer 4 at B*/
B_input(packet)
  struct pkt packet;
{
  char buf[20] = {0};
  strncpy(buf,packet.payload,20);
  struct msg message;
  printf("B receive: seq:%d,need:%d\n",packet.seqnum,Breceive);//getchar();

  if(packet.checksum == generateChecksum(packet) )
  { 
    if(packet.seqnum < Breceive)
    {
      struct pkt ret = generatePkg(message,OK,Breceive);
      tolayer3(B_SEND,ret);
    }

    if (packet.seqnum == Breceive)
    {
      strncpy(message.data,buf,20);
      tolayer5(B_SEND,message);

      //改成累计确认
      struct pkt ret = generatePkg(message,OK,Breceive);
      Breceive++;
      tolayer3(B_SEND,ret);
    }
  }
  else
  {

    //不返回，等待超时
    // struct pkt ret = generatePkg(message,ERROR,packet.seqnum);
    // tolayer3(B_SEND,ret);

    // P_ERROR("B get error");
  }
};

B_output(message)  /* need be completed only for extra credit */
  struct msg message;
{
}

/* called when B's timer goes off */
B_timerinterrupt()
{
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
B_init()
{
  
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */ 
         /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */   
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;
   
   int i,j;
   char c; 
  
   init();
   A_init();
   B_init();
   
   while (1) {
        eventptr = evlist; 

        if (eventptr==NULL)
            goto terminate;

        evlist = evlist->next;  

        if (evlist!=NULL)
            evlist->prev=NULL;
        if (TRACE>=2) 
        {
          printf("\nEVENT time: %f,",eventptr->evtime);
          printf("  type: %d",eventptr->evtype);
          if (eventptr->evtype==0)
          printf(", timerinterrupt  ");
            else if (eventptr->evtype==1)
              printf(", fromlayer5 ");
            else
          printf(", fromlayer3 ");
          printf(" entity: %d\n",eventptr->eventity);
          }
        
        time = eventptr->evtime;        /* update time to next event time */



        if (nsim==nsimmax)
	        break;           



        if (eventptr->evtype == FROM_LAYER5 ) 
        {
            generate_next_arrival(); 
            j = nsim % 26; 
            for (i=0; i<20; i++)  
                msg2give.data[i] = 97 + j;

            if (TRACE>2) 
            {
              printf("          MAINLOOP: data given to student: ");
                for (i=0; i<20; i++) 
                printf("%c", msg2give.data[i]);
              printf("\n");
	          }


            nsim++;

            if (eventptr->eventity == A) 
                A_output(msg2give);  
              else
                B_output(msg2give);  
            }

          else if (eventptr->evtype ==  FROM_LAYER3) 
          {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;

            for (i=0; i<20; i++)  
                pkt2give.payload[i] = eventptr->pktptr->payload[i];

            if (eventptr->eventity ==A)      /* deliver packet by calling */
                A_input(pkt2give);            /* appropriate entity  */
                  else
                B_input(pkt2give);
            free(eventptr->pktptr);          /* free the memory for packet */
                  }
                else if (eventptr->evtype ==  TIMER_INTERRUPT) {
                  if (eventptr->eventity == A) 
              A_timerinterrupt();
                  else
              B_timerinterrupt();
                  }
                else  
                {
                  printf("INTERNAL PANIC: unknown event type \n");
                }
              //free(eventptr);
        }
  

  


terminate:
    printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time,nsim);
}



init()                         /* initialize the simulator */
{
  int i;
  float sum, avg;
  float jimsrand();
  
  if(DEBUG)
  {
    nsimmax = 50;
    lossprob =0.2;
    corruptprob = 0.2;
    lambda = 50;
    TRACE = 0;
  }

  else{
    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
    printf("Enter the number of messages to simulate: ");
    scanf("%d",&nsimmax);
    printf("Enter  packet loss probability [enter 0.0 for no loss]:");
    scanf("%f",&lossprob);
    printf("Enter packet corruption probability [0.0 for no corruption]:");
    scanf("%f",&corruptprob);
    printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
    scanf("%f",&lambda);
    printf("Enter TRACE:");
    scanf("%d",&TRACE);
}
   srand(9999);              /* init random number generator */
   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
   avg = sum/1000.0;
   if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" ); 
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(0);//exit func need update
    }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   time=0.0;                    /* initialize time to 0.0 */
   generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand() 
{
  double mmm = 2147483647 ;       /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;
  #ifdef _WIN32                  /* individual students may need to change mmm */ 
    mmm = 0x7fff;
    x = rand()/mmm;
  #elif __linux__
    x = rand()/mmm;
  #endif                          /* x should be uniform in [0,1] */
  //printf("%f\n",x);
  return(x);
}  

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
    char *malloc();
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
   x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + x;
   evptr->evtype =  FROM_LAYER5;
   if (BIDIRECTIONAL && (jimsrand()>0.5) )
      evptr->eventity = B;
    else
      evptr->eventity = A;
   insertevent(evptr);
} 


insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

printevlist()
{
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q,*qold;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

 struct event *q;
 struct event *evptr;
 char *malloc();

 if (TRACE>2)
    printf("START TIMER: starting timer at %f\n",time);
   for (q=evlist; q!=NULL ; q = q->next)  
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }
 
/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
} 


/************************** TOLAYER3 ***************/
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 char *malloc();
 float lastime, x, jimsrand();
 int i;


 ntolayer3++;

 /* simulate losses: */
 if (jimsrand() < lossprob)  {
      nlost++;
      if (TRACE>0)    
	printf("          TOLAYER3: packet being lost\n");
      return;
    }  

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */ 
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = time;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) ) 
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*jimsrand();
 


 /* simulate corruption: */
 if (jimsrand() < corruptprob)  {
    ncorrupt++;
    if ( (x = jimsrand()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)    
	printf("          TOLAYER3: packet being corrupted\n");
    }  

  if (TRACE>2)  
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
} 

tolayer5(AorB,datasent)
  int AorB;
  char datasent[20];
{
  int i;  
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)  
        printf("%c",datasent[i]);
     printf("\n");
   }
  
}
