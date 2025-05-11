This module provides an event queue mechanism optimized for microcontrollers. The usage is similar to Qt's signals and slots. 

First, you need to create a queue object. 

    modern_framework::task_queue(void *pbuffer, uint16_t pbuffer_len, void *qbuffer, uint16_t queue_len)

The space for the two buffers needs to be allocated by yourself.

    task_queue::bind

can connect signals and slots. 

Signals and slots must be class members, where the type of the signal is 
    
    modern_framework::signal<function type>
    
On the other hand, the implementation of the slot function should be a member function of the same type, for example:

    class has_signals
    {
    public:
        modern_framework::signal<int(int,myclass&&)> sgn;
        modern_framework::signal<void(has_signal&)> sgn2;
    };
    
    class has_slots
    {
    public:
        int slot_fun(int p,myclass&& q)
        {
            printf("p=%d,q.k=%f,q.l=%d\n",p,q.k,q.l);
            return 0;
        }
        void slot_fun2(has_signal& b)
        {
            puts(__FUNCTION__);
            b.sgn2(b);
        }
    };

'myclass' is a custom type, defined as follows:

    class myclass
    {
    public:
        double k;
        int l;
        myclass(int a,double b){l=a,k=b;puts("ctor with args\n");}
        myclass(const myclass& that){k=that.k;l=that.l;puts("cp ctor\n");}
        myclass(myclass&& that){k=that.k;l=that.l;puts("mv ctor\n");}
        ~myclass(){puts("~ctor\n");}
    };

The parameters of signal and slots support any type except union, including

    T //The basic typed or class typed object will be copied into the queue, and when the slot function is called, it will be copied to the function again.
    T* //Any level pointer,only pointer will be copied
    T& //The pointer will be copied into the queue, so you need to manage the validity of the object yourself
    T&& //The object is moved to the queue and moved to the function again when the slot function is called
    const T& //The referenced object will be copied into the queue but will not be copied again when the slot called

The return value is usually void. If it is any other type, there must be a default constructor. When the signal is delivered, the default object will be returned immediately, so the return value is usually meaningless.

Signals and slots can be bound using the following methods:

    has_signals b;
    has_slots c;
    queue.bind(&has_signals::sgn,&b,MKSLOT(&has_slots::slot_fun),&c);
    queue.bind(&has_signals::sgn2,&b,MKSLOT(&has_slots::slot_fun2),&c);
    
Then you can deliver

    b.sgn(1,myclass(2,114.514));
    b.sgn2(b);

Events can be polled in an infinite loop

    while(true)
        queue.process();

Important: This is an early version, and the correctness of some logic has not been verified, such as when the queue is full, and there may be unsafe issues when interrupting delivery.
