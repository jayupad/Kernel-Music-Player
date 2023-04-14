// #ifndef _shared_h_
// #define _shared_h_

// #include "debug.h"

// template <typename T>
// class Shared {
//     T* ptr;

// public:

//     explicit Shared(T* it) : ptr(it) {
//         MISSING();
//     }

//     //
//     // Shared<Thing> a{};
//     //
//     Shared(): ptr(nullptr) {
//         MISSING();
//     }

//     //
//     // Shared<Thing> b { a };
//     // Shared<Thing> c = b;
//     // f(b);
//     // return c;
//     //
//     Shared(const Shared& rhs): ptr(rhs.ptr) {
//         MISSING();
//     }

//     //
//     // Shared<Thing> d = g();
//     //
//     Shared(Shared&& rhs): ptr(rhs.ptr) {
//         rhs.ptr = nullptr;
//     }

//     ~Shared() {
//         MISSING();
//     }

//     // d->m();
//     T* operator -> () const {
//         return ptr;
//     }

//     // d = nullptr;
//     // d = new Thing{};
//     Shared<T>& operator=(T* rhs) {
//         MISSING();
//         return *this;
//     }

//     // d = a;
//     // d = Thing{};
//     Shared<T>& operator=(const Shared<T>& rhs) {
//         MISSING();
//         return *this;
//     }

//     // d = g();
//     Shared<T>& operator=(Shared<T>&& rhs) {
//         MISSING();
//         return *this;
//     }

//     bool operator==(const Shared<T>& rhs) const {
//         MISSING();
//         return false;
//     }

//     bool operator!=(const Shared<T>& rhs) const {
//         MISSING();
//         return false;
//     }

//     bool operator==(T* rhs) {
//         MISSING();
//         return false;
//     }

//     bool operator!=(T* rhs) {
//         MISSING();
//         return false;
//     }

//     // e = Shared<Thing>::make(1,2,3);
//     template <typename... Args>
//     static Shared<T> make(Args... args) {
//         return Shared<T>{new T(args...)};
//     }

// };

// #endif
#ifndef _shared_h_
#define _shared_h_

#include "debug.h"
#include "smp.h"
static ISL spin;
template <typename T>
class Shared {
    T* ptr;


public:

    explicit Shared(T* it) : ptr(it) {
        if(it != nullptr) {
            (ptr->ref_count).fetch_add(1);
        }
    }

    //
    // Shared<Thing> a{};
    //
    Shared(): ptr(nullptr) {
    }

    //
    // Shared<Thing> b { a };
    // Shared<Thing> c = b;
    // f(b);
    // return c;
    //
    Shared(const Shared& rhs): ptr(rhs.ptr) {
        if(rhs.ptr != nullptr) {
            (ptr->ref_count).fetch_add(1);
        }
    }

    //
    // Shared<Thing> d = g();
    //
    Shared(Shared&& rhs): ptr(rhs.ptr) {
        rhs.ptr = nullptr;
    }

    ~Shared() {
        if(ptr != nullptr) {
            if((ptr->ref_count).add_fetch(-1) == 0) {
                delete ptr; 
            }
        }
    }

    // d->m();
    T* operator -> () const {
        return ptr;
    }

    // d = nullptr;
    // d = new Thing{};
    Shared<T>& operator=(T* rhs) {

        if(ptr == rhs) {
            return *this;
        }

        T* holder = ptr; 

        ptr = rhs; 
        if(ptr != nullptr) {
            (ptr->ref_count).fetch_add(1);
        }

        if(holder != nullptr) {
            T* temp = holder; 
            if((holder->ref_count).add_fetch(-1) == 0) {
                delete temp; 
            }
        }

        return *this;
    }

    // d = a;
    // d = Thing{};
    Shared<T>& operator=(const Shared<T>& rhs) {

        if(ptr == rhs.ptr) {
            return *this;
        }

        T* holder = ptr; 

        ptr = rhs.ptr; 
        if(ptr != nullptr) {
            (ptr->ref_count).fetch_add(1);
        }

        if(holder != nullptr) {
            T* tempr = holder; 
            if((holder->ref_count).add_fetch(-1) == 0) {
                delete tempr; 
            }
        }

        return *this;
    }

    // d = g();
    Shared<T>& operator=(Shared<T>&& rhs) {

        if(ptr == rhs.ptr) {
            return *this;
        }

        T* holder = ptr; 


        ptr = rhs.ptr; 
        rhs.ptr = nullptr;

        if(holder != nullptr) {
            T* temp = holder; 
            if((holder->ref_count).add_fetch(-1) == 0) {
                delete temp; 
            }
        }

        return *this;
    }

    bool operator==(const Shared<T>& rhs) const {
        return ptr == rhs.ptr;
    }

    bool operator!=(const Shared<T>& rhs) const {
        return ptr != rhs.ptr;
    }

    bool operator==(T* rhs) {
        return ptr == rhs;
    }

    bool operator!=(T* rhs) {
        return ptr != rhs;
    }

    // e = Shared<Thing>::make(1,2,3);
    template <typename... Args>
    static Shared<T> make(Args... args) {
        return Shared<T>{new T(args...)};
    }

};

#endif