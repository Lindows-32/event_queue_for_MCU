#include "event_queue.h"
using namespace modern_framework;
bool task_queue::post(standard_fun *fun, void *pthis)
{
    uint16_t new_end=_end+1;
    if(new_end==_queue_len)
        new_end=0;
    if(new_end==_begin)
        return false;

    _queue_buffer[_end].fun=fun;
    _queue_buffer[_end].pthis=pthis;
    _end=new_end;
    return true;
}

void task_queue::process()
{
    if(_begin!=_end)
    {
        _queue_buffer[_begin].fun(_queue_buffer[_begin].pthis,&_param_buffer);
        _begin++;
        if(_begin==_queue_len)
            _begin=0;
    }
}

void param_buffer::release()
{
    auto p=reinterpret_cast<param_node_base*>(_buffer+_begin);
    if(p->deleter)
        p->deleter(_buffer+_begin+p->offset);

    _begin=p->next;
    if(_begin==0xFFFF)
        _begin=_end;

}

void param_buffer::cancel()
{
    auto p=reinterpret_cast<param_node_base*>(_buffer+_end_addr);
    if(p->deleter)
        p->deleter(_buffer+_end_addr+p->offset);

    if(_end_addr==_begin)
        _end=_begin;
    else
    {
        auto q=reinterpret_cast<param_node_base*>(_buffer+p->prev);
        _end_addr=p->prev;
        _end=p->prev+q->size;
    }
}

