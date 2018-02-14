#ifndef SINGLETON_H_
#define SINGLETON_H_

template <class T>
class Singleton
{
protected:
    static T* pSingleton;

public:
    Singleton()
    {
        pSingleton = static_cast<T*>(this);
    }

    ~Singleton()
    {
        pSingleton = nullptr;
    }

    static T* GetPointer()
    {
        return pSingleton;
    }

    static T& GetInstance()
    {
        return *pSingleton;
    }
};

template <class T>
T* Singleton<T>::pSingleton = nullptr;

#endif // SINGLETON_H_