#ifndef __SMART_PTR_DEBUGGER_H__
#define __SMART_PTR_DEBUGGER_H__

#include "util/app.h"
#include "debug.h"

#define DECLARE_SMART_CLASS_DEBUGGER(clsname) \
	class clsname; \
	typedef SmartPtrDebugger<clsname> clsname ## Ptr;

BEGIN_APP_NAMESPACE

class SmartObjectDebugger
{
protected:
	mutable unsigned int  theRefCount;

public:
	SmartObjectDebugger() : theRefCount(0)
	{
		DMSG("SmartObjectDebugger: " << (void*)this << ": allocated");
	}

	SmartObjectDebugger(const SmartObjectDebugger&) : theRefCount(0) { }

	virtual ~SmartObjectDebugger()
	{
		DMSG("SmartObjectDebugger: " << (void*)this << ": deleted");
	}

	virtual void free()
	{
		delete this;
	}

	long getRefCount() const
	{
		return theRefCount;
	}

	void addReference() const
	{
		++theRefCount;
		DMSG("SmartObjectDebugger: " << (void*)this << ": incremented to " << theRefCount);
	}

	void removeReference ()
	{
		--theRefCount;
		DMSG("SmartObjectDebugger: " << (void*)this << ": decremented to " << theRefCount);
		if (theRefCount == 0)
			free();
	}

	SmartObjectDebugger& operator=(const SmartObjectDebugger&)
	{
		return *this;
	}
}; /* class SmartObjectDebugger */

template<class T>
class SmartPtrDebugger
{
protected:
	T  *p;

	void init()
	{
		if (p != 0)
		{
			DMSG(p->getRefCount() << ": " << ((void*)this) << " add reference of " << ((void*)p) << " at: ");
			Util::MainApplication::printStackTrace(std::cerr);
			p->addReference();
		}
	}

	template <class otherT> SmartPtrDebugger&
	assign (const SmartPtrDebugger<otherT>& rhs)
	{
		if (p != rhs.get())
		{
			if (p)
			{
				DMSG(p->getRefCount() << ": " << ((void*)this) << " remove reference of " << ((void*)p) << " (per copy) at: ");
				Util::MainApplication::printStackTrace(std::cerr);
				p->removeReference();
			}
			p = static_cast<T*>(rhs.get());
			init();
		}
		return *this;
	}

public:
	SmartPtrDebugger(T* realPtr = 0) : p(realPtr)
	{
		init();
	}

	SmartPtrDebugger(SmartPtrDebugger const& rhs) : p(rhs.get())
	{
		init();
	}

	~SmartPtrDebugger() throw ()
	{
		if (p)
		{
			DMSG(p->getRefCount() << ": " << ((void*)this) << " remove reference of " << ((void*)p) << " (per dtor) at: ");
			Util::MainApplication::printStackTrace(std::cerr);
			p->removeReference();
		}
		p = 0;
	}

	bool isNull () const
	{
		return p == 0;
	}

	T* get() const
	{
		return p;
	}

	operator T* ()
	{
		return get();
	}
	operator const T * () const
	{
		return get();
	}

	T* operator->() const
	{
		return p;
	}
	T& operator*() const
	{
		return *p;
	}

	bool operator==(SmartPtrDebugger const& h) const
	{
		return p == h.p;
	}
	bool operator!=(SmartPtrDebugger const& h) const
	{
		return p != h.p;
	}
	bool operator==(T const* pp) const
	{
		return p == pp;
	}
	bool operator!=(T const* pp) const
	{
		return p != pp;
	}
	bool operator<(const SmartPtrDebugger& h) const
	{
		return p < h.p;
	}

	SmartPtrDebugger& operator=(SmartPtrDebugger const& rhs)
	{
		return assign (rhs);
	}

	template <class otherT> SmartPtrDebugger& operator=(SmartPtrDebugger<otherT> const& rhs)
	{
		return assign (rhs);
	}

};  /* SmartPtrDebugger */

END_APP_NAMESPACE

#endif // __SMART_PTR_DEBUGGER_H__

