#ifndef SINGLETON_H_
#define SINGLETON_H_

template <class T>
class Singleton
{
protected:
	static T* ms_pSingleton;

public:
	Singleton()
	{
		ms_pSingleton = static_cast<T*>(this);
	}

	~Singleton()
	{
		ms_pSingleton = nullptr;
	}

	static T* pointer()
	{
		return ms_pSingleton;
	}
};

template <class T>
T* Singleton<T>::ms_pSingleton = nullptr;

#endif // SINGLETON_H_