#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H
#include <stdint.h>
#include <new>

namespace modern_framework
{
    template <typename _Tp>
    struct remove_reference
    {
        using type = _Tp;
    };

    template <typename _Tp>
    struct remove_reference<_Tp &>
    {
        using type = _Tp;
    };

    template <typename _Tp>
    struct remove_reference<_Tp &&>
    {
        using type = _Tp;
    };

    template <typename _Tp>
    constexpr typename remove_reference<_Tp>::type&& move(_Tp &&__t) noexcept
    {
        return static_cast<typename remove_reference<_Tp>::type&&>(__t);
    }

    template <typename _Tp>
    constexpr _Tp && forward(typename remove_reference<_Tp>::type& __t) noexcept
    {
        return static_cast<_Tp&&>(__t);
    }

    template <typename _Tp>
    constexpr _Tp && forward(typename remove_reference<_Tp>::type&& __t) noexcept
    {
        return static_cast<_Tp&&>(__t);
    }

    template <class T>
    class all2const_ref
    {
    public:
        using type = const T &;
    };

    template <class T>
    class all2const_ref<T &&>
    {
    public:
        using type = const T &;
    };

    template <class T>
    class all2const_ref<T &>
    {
    public:
        using type = const T &;
    };

    template <template <class... _args> class template_type, class specialized_type, class... args>
    class if_args_not_empty
    {
    public:
        using type = template_type<args...>;
    };

    template <template <class... _args> class template_type, class specialized_type>
    class if_args_not_empty<template_type, specialized_type>
    {
    public:
        using type = specialized_type;
    };

    template <template <class T> class _modify_rule, template <class... _function_args> class _callback_class, class... _fixed_args>
    class parameters_pack_modify
    {
        template <class T>
        using F = typename _modify_rule<T>::type;

        template <class... args>
        class parameter_pack_expansion;

        template <class T, class... pargs>
        class parameter_pack_expansion<T, pargs...>
        {
        public:
            using head_type = T;
            using next = parameter_pack_expansion<pargs...>;
            template <class... cargs>
            class construct_new_parameter_pack
            {
            public:
                using result_type = typename next::template construct_new_parameter_pack<cargs..., F<typename next::head_type>>::result_type;
            };

            class begin
            {
            public:
                using result_type = typename construct_new_parameter_pack<F<T>>::result_type;
            };
        };

        template <class T>
        class parameter_pack_expansion<T>
        {
        public:
            using head_type = T;
            template <class... cargs>
            class construct_new_parameter_pack
            {
            public:
                using result_type = construct_new_parameter_pack;
                using type = _callback_class<_fixed_args..., cargs...>;
                inline static void exec(_fixed_args... fixed_args, cargs... args)
                {
                    _callback_class<cargs...>::exec(forward<_fixed_args>(fixed_args)..., forward<cargs>(args)...);
                }
            };
            class begin
            {
            public:
                using result_type = typename construct_new_parameter_pack<F<T>>::result_type;
            };
        };

        class parameter_pack_expansion_no_args
        {
            class construct_new_parameter_pack
            {
            public:
                using result_type = construct_new_parameter_pack;
                using type = _callback_class<>;
                inline static void exec(_fixed_args... fixed_args)
                {
                    _callback_class<>::exec(fixed_args...);
                }
            };

        public:
            class begin
            {
            public:
                using result_type = typename construct_new_parameter_pack::result_type;
            };
        };

    public:
        template <class... _type_args>
        using modify = typename if_args_not_empty<parameter_pack_expansion, parameter_pack_expansion_no_args, _type_args...>::type::begin::result_type::result_type;
    };

    template <typename T>
    struct is_class
    {
    private:
        class true_type
        {
        public:
            constexpr static bool value = true;
        };

        class false_type
        {
        public:
            constexpr static bool value = false;
        };

        template <typename U>
        static true_type test(int U::*);

        template <typename U>
        static false_type test(...);
        using type = decltype(test<T>(nullptr));

    public:
        constexpr static bool value = type::value;
    };

    class param_node_base
    {
    public:
        void (*deleter)(void *ptr);
        uint16_t prev;
        uint16_t next;
        uint16_t size;
        uint16_t offset;
    };

    template <class T>
    class param_node_t : public param_node_base
    {
        template <class C, bool c = is_class<C>::value>
        struct test_deleter
        {
            static void exec(void *ptr)
            {
                static_cast<C *>(ptr)->~C();
            }
        };

        template <class C>
        struct test_deleter<C, false>
        {
            constexpr static void (*exec)(void *) = nullptr;
        };

    public:
        using type = T;
        uint8_t value[sizeof(T)];

        template <class Src>
        void copy(const Src &s)
        {
            new (value) T(s);
            deleter = test_deleter<T>::exec;
        }
        template <class Src>
        void move(Src &&s)
        {
					new (value) T(modern_framework::move(s));
            deleter = test_deleter<T>::exec;
        }

        void create()
        {
            new (value) T();
            deleter = test_deleter<T>::exec;
        }
        T &get() { return *reinterpret_cast<T *>(value); }
    };

    class param_buffer
    {
        uint8_t *_buffer;
        uint16_t buffer_len;
        uint16_t _begin;
        uint16_t _end;
        uint16_t _end_addr;

    public:
        param_buffer(void *buffer, int length)
        {
            _begin = 0;
            _end = 0;
            _buffer = static_cast<uint8_t *>(buffer);
            buffer_len = length;
        }
        template <class T>
        param_node_t<T> *get(uint16_t addr)
        {
            return reinterpret_cast<param_node_t<T> *>(_buffer + addr);
        }
        template <class T>
        uint16_t take()
        {

            if (_end == _begin)
            {
                _begin = 0;
                _end = 0;
                _end_addr=0;
            }

            uint16_t this_begin = _end;
            auto take_initial = [this, &this_begin]() -> uint16_t
            {
                auto p = reinterpret_cast<param_node_t<T> *>(_buffer + this_begin);

                if (_end != _begin)
                {
                    p->prev = _end_addr;
                    reinterpret_cast<param_node_t<T> *>(_buffer + _end_addr)->next = this_begin;
                }
                else
                    p->prev = 0xFFFF;
                p->next = 0xFFFF;
                p->size = sizeof(param_node_t<T>);
                p->deleter = nullptr;
                p->offset = reinterpret_cast<uint8_t *>(&p->value) - (_buffer + this_begin);
                _end_addr = this_begin;
                _end = this_begin + sizeof(param_node_t<T>);
                return this_begin;
            };
            if (this_begin >= _begin)
            {
                if (this_begin + sizeof(param_node_t<T>) < buffer_len)
                    return take_initial();
                else
                    this_begin = 0;
            }
            if (this_begin <= _begin)
            {
                if (this_begin + sizeof(param_node_t<T>) >= _begin)
                    return 0xFFFF;
                else
                    return take_initial();
            }
            return 0xFFFF;
        }
        void release();
        uint16_t begin() { return _begin; }
        uint16_t end() { return _end_addr; }
        void cancel();
        bool empty() { return _begin == _end; }
    };

    using standard_fun = void(void *pthis, param_buffer *p);
    class queue_node
    {
    public:
        standard_fun *fun;
        void *pthis;
    };

    template <uint16_t len>
    using queue_buffer = queue_node[len];

    class task_queue
    {
        param_buffer _param_buffer;
        queue_node *_queue_buffer;
        uint16_t _param_buffer_len;
        uint16_t _queue_len;
        uint16_t _begin;
        uint16_t _end;
        template <class...>
        friend class post_helper;

    public:
        task_queue(void *pbuffer, uint16_t pbuffer_len, void *qbuffer, uint16_t queue_len)
            : _param_buffer(pbuffer, pbuffer_len)
        {
            _queue_buffer = static_cast<queue_node *>(qbuffer);
            _queue_len = queue_len;
            _begin = 0;
            _end = 0;
        }

        bool post(standard_fun *fun, void *pthis);
        param_buffer &get_param_buffer() { return _param_buffer; }
        template <class S, class C1, class SLOT, class C2>
        void bind(S sig, C1 *sender, SLOT, C2 *reciever)
        {
            (sender->*sig)._task_queue = this;
            (sender->*sig)._pthis = reciever;
            (sender->*sig)._fun = SLOT::exec;
            (sender->*sig)._direct_call = SLOT::to_c_style_function;
        }
        void process();
    };

    template <class... ARGS>
    class param_packager;

    template <>
    class param_packager<>
    {
    public:
        template <class... _ARGS>
        class post_helper
        {
        public:
            static bool exec(task_queue *queue, standard_fun *fun, void *pthis)
            {
                return queue->post(fun, pthis);
            }
        };
    };

    template <class T, class... ARGS>
    class param_packager<T, ARGS...> // T value => copy constructor
    {
    public:
        template <class... _ARGS>
        class post_helper;

        template <class _T, class... _ARGS>
        class post_helper<const _T &, _ARGS...>
        {
        public:
            static bool exec(task_queue *queue, standard_fun *fun, void *pthis, const T &t, _ARGS... args)
            {
                uint16_t addr = queue->get_param_buffer().take<T>();
                if (addr == 0xFFFF)
                    return false;
                if (param_packager<ARGS...>::template post_helper<_ARGS...>::exec(queue, fun, pthis, forward<_ARGS>(args)...))
                {
                    queue->get_param_buffer().get<T>(addr)->copy(t);
                    return true;
                }
                else
                {
                    queue->get_param_buffer().cancel();
                    return false;
                }
            }
        };
    };

    template <class T, class... ARGS>
    class param_packager<T &&, ARGS...> // T r_ref => move
    {
    public:
        template <class... _ARGS>
        class post_helper;

        template <class _T, class... _ARGS>
        class post_helper<const _T &, _ARGS...>
        {
        public:
            static bool exec(task_queue *queue, standard_fun *fun, void *pthis, const T &t, _ARGS... args)
            {
                uint16_t addr = queue->get_param_buffer().take<T>();
                if (addr == 0xFFFF)
                    return false;
                if (param_packager<ARGS...>::template post_helper<_ARGS...>::exec(queue, fun, pthis, forward<_ARGS>(args)...))
                {
                    queue->get_param_buffer().get<T>(addr)->move(modern_framework::move(const_cast<T &>(t)));
                    return true;
                }
                else
                {
                    queue->get_param_buffer().cancel();
                    return false;
                }
            }
        };
    };

    template <class T, class... ARGS>
    class param_packager<T &, ARGS...> // T ref => transmit pointer
    {
    public:
        template <class... _ARGS>
        class post_helper;

        template <class _T, class... _ARGS>
        class post_helper<const _T &, _ARGS...>
        {
        public:
            static bool exec(task_queue *queue, standard_fun *fun, void *pthis, const T &t, _ARGS... args)
            {
                uint16_t addr = queue->get_param_buffer().take<T *>();
                if (addr == 0xFFFF)
                    return false;
                if (param_packager<ARGS...>::template post_helper<_ARGS...>::exec(queue, fun, pthis, forward<_ARGS>(args)...))
                {
                    queue->get_param_buffer().get<T *>(addr)->copy(const_cast<T *>(&t));
                    return true;
                }
                else
                {
                    queue->get_param_buffer().cancel();
                    return false;
                }
            }
        };
    };

    // const T& => cp object
    template <class T, class... ARGS>
    class param_packager<const T &, ARGS...> // T ref => transmit pointer
    {
    public:
        template <class... _ARGS>
        class post_helper;

        template <class _T, class... _ARGS>
        class post_helper<const _T &, _ARGS...>
        {
        public:
            static bool exec(task_queue *queue, standard_fun *fun, void *pthis, const T &t, _ARGS... args)
            {
                uint16_t addr = queue->get_param_buffer().take<T>();
                if (addr == 0xFFFF)
                    return false;
                if (param_packager<ARGS...>::template post_helper<_ARGS...>::exec(queue, fun, pthis, forward<_ARGS>(args)...))
                {
                    queue->get_param_buffer().get<T>(addr)->move(modern_framework::move(const_cast<T &>(t)));
                    return true;
                }
                else
                {
                    queue->get_param_buffer().cancel();
                    return false;
                }
            }
        };
    };

    class signal_base
    {
    protected:
        friend class task_queue;
        task_queue *_task_queue;
        void *_pthis;
        standard_fun *_fun;
    };

    template <class T>
    class signal;

    template <class R, class... ARGS>
    class signal<R(ARGS...)> : public signal_base
    {
        friend class task_queue;
        R (*_direct_call)(void *, ARGS...);

    public:
        R operator()(ARGS... args)
        {
            using ty = typename parameters_pack_modify<all2const_ref, param_packager<ARGS...>::template post_helper>::template modify<ARGS...>::result_type::result_type::type;
            ty::exec(_task_queue, _fun, _pthis, args...);
            return R();
        }
        R syncall(ARGS... args)
        {
            return _direct_call(_pthis, forward<ARGS>(args)...);
        }
        signal()
        {
            _task_queue = nullptr;
            _pthis = nullptr;
            _fun = nullptr;
            _direct_call = nullptr;
        }
        ~signal()
        {
        }
    };
    template <class... ARGS>
    class signal<void(ARGS...)> : public signal_base
    {
        friend class task_queue;
        void (*_direct_call)(void *, ARGS...);

    public:
        signal()
        {
            _task_queue = nullptr;
            _pthis = nullptr;
            _fun = nullptr;
            _direct_call = nullptr;
        }
        ~signal()
        {
        }
        void operator()(ARGS... args)
        {
            using ty = typename parameters_pack_modify<all2const_ref, param_packager<ARGS...>::template post_helper>::template modify<ARGS...>::result_type::result_type::type;
            ty::exec(_task_queue, _fun, _pthis, args...);
        }
        void syncall(ARGS... args)
        {
            _direct_call(_pthis, forward<ARGS>(args)...);
        }
    };

    template <class T>
    struct maybe_move
    {
        static constexpr T exec(const T &t) { return t; }
    };

    template <class T>
    struct maybe_move<T &&>
    {

        static constexpr T &&exec(const T &t) { return static_cast<T &&>(const_cast<T &>(t)); }
    };

    template <class T>
    struct maybe_move<T &>
    {
        static constexpr T &exec(const T &t) { return const_cast<T &>(t); }
    };

    template <class T>
    struct maybe_move<const T &>
    {
        static constexpr const T &exec(const T &t) { return t; }
    };

    template <class T>
    struct invoke_helper;

    template <class R, class C, class... ARGS>
    struct invoke_helper<R (C::*)(ARGS...)>
    {
        template <R (C::*member_function)(ARGS...)>
        struct with_fun
        {
            template <class NOUSE, class... _ARGS>
            class with_origin_type;

            template <class NOUSE, class T, class... _ARGS>
            class with_origin_type<NOUSE, T, _ARGS...>
            {
            public:
                template <class... __ARGS>
                class final_type
                {
                public:
                    static void invoke(void *pthis, param_buffer *pbf, uint16_t addr, __ARGS... args)
                    {
                        param_node_t<T> *data = pbf->get<T>(addr == 0xFFFF ? pbf->begin() : addr);
                        with_origin_type<NOUSE, _ARGS...>::template final_type<__ARGS..., typename all2const_ref<T>::type>::invoke(pthis, pbf, data->next, forward<__ARGS>(args)..., data->get());
                    }
                };
            };

            template <class NOUSE, class T, class... _ARGS>
            class with_origin_type<NOUSE, T &&, _ARGS...>
            {
            public:
                template <class... __ARGS>
                class final_type
                {
                public:
                    static void invoke(void *pthis, param_buffer *pbf, uint16_t addr, __ARGS... args)
                    {
                        param_node_t<T> *data = pbf->get<T>(addr == 0xFFFF ? pbf->begin() : addr);
                        with_origin_type<NOUSE, _ARGS...>::template final_type<__ARGS..., typename all2const_ref<T>::type>::invoke(pthis, pbf, data->next, forward<__ARGS>(args)..., data->get());
                    }
                };
            };

            template <class NOUSE, class T, class... _ARGS>
            class with_origin_type<NOUSE, T &, _ARGS...>
            {
            public:
                template <class... __ARGS>
                class final_type
                {
                public:
                    static void invoke(void *pthis, param_buffer *pbf, uint16_t addr, __ARGS... args)
                    {
                        param_node_t<T *> *data = pbf->get<T *>(addr == 0xFFFF ? pbf->begin() : addr);
                        with_origin_type<NOUSE, _ARGS...>::template final_type<__ARGS..., typename all2const_ref<T>::type>::invoke(pthis, pbf, data->next, forward<__ARGS>(args)..., *data->get());
                    }
                };
            };

            template <class NOUSE, class T, class... _ARGS>
            class with_origin_type<NOUSE, const T &, _ARGS...>
            {
            public:
                template <class... __ARGS>
                class final_type
                {
                public:
                    static void invoke(void *pthis, param_buffer *pbf, uint16_t addr, __ARGS... args)
                    {
                        param_node_t<T> *data = pbf->get<T>(addr == 0xFFFF ? pbf->begin() : addr);
                        with_origin_type<NOUSE, _ARGS...>::template final_type<__ARGS..., typename all2const_ref<T>::type>::invoke(pthis, pbf, data->next, forward<__ARGS>(args)..., data->get());
                    }
                };
            };

            template <class NOUSE>
            class with_origin_type<NOUSE>
            {
            public:
                template <class... __ARGS>
                class final_type
                {
                public:
                    static void invoke(void *pthis, param_buffer *pbf, uint16_t, __ARGS... args)
                    {

                        (static_cast<C *>(pthis)->*member_function)(maybe_move<ARGS>::exec(args)...);
                        for (int32_t i = 0; i < sizeof...(__ARGS); i++)
                        {
                            pbf->release();
                        }
                    }
                };
            };
        };
    };

    template <class T>
    class from_member_function;

    template <class C, class... ARGS>
    class from_member_function<void (C::*)(ARGS...)>
    {
    public:
        template <void (C::*member_fun)(ARGS...)>
        class to_slot_function
        {
        public:
            static void exec(void *pthis, param_buffer *pbf)
            {
                invoke_helper<void (C::*)(ARGS...)>::template with_fun<member_fun>::template with_origin_type<void, ARGS...>::template final_type<>::invoke(pthis, pbf, 0xFFFF);
            }

            static void to_c_style_function(void *pthis, ARGS... args)
            {
                (static_cast<C *>(pthis)->*member_fun)(forward<ARGS>(args)...);
            }
        };
    };

    template <class R, class C, class... ARGS>
    class from_member_function<R (C::*)(ARGS...)>
    {
    public:
        template <R (C::*member_fun)(ARGS...)>
        class to_slot_function
        {
        public:
            static void exec(void *pthis, param_buffer *pbf)
            {
                invoke_helper<R (C::*)(ARGS...)>::template with_fun<member_fun>::template with_origin_type<void, ARGS...>::template final_type<>::invoke(pthis, pbf, 0xFFFF);
            }

            static R to_c_style_function(void *pthis, ARGS... args)
            {
                return (static_cast<C *>(pthis)->*member_fun)(forward<ARGS>(args)...);
            }
        };
    };
}

#define MKSLOT(member_fun) from_member_function<decltype(member_fun)>::to_slot_function<member_fun>()
#endif // EVENT_QUEUE_H
