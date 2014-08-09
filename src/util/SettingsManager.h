#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <boost/thread.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <vector>
#include <map>

#include "Filesystem.h"

namespace settingsmanager {
	// NOTE: Use class specializations intead of function specializations
	//       see http://www.gotw.ca/publications/mill17.htm

	template <typename T>
	struct fromstring_t {
		static void convert(const std::string& in, T& out)
		{
			out = boost::lexical_cast<T>(in);
		}
	};

	template <typename T>
	struct tostring_t {
		static void convert(const T& in, std::string& out)
		{
			out = boost::lexical_cast<std::string>(in);
		}
	};

	template <typename T>
	inline void fromstring(const std::string& in, T& out)
	{
		fromstring_t<T>::convert(in, out);
	}

	template <typename T>
	inline void tostring(const T& in, std::string& out)
	{
		tostring_t<T>::convert(in, out);
	}
}

class SettingsVariableBase
{
	protected:
		mutable boost::mutex mutex;

		SettingsVariableBase& operator=(const SettingsVariableBase& svb)
		{
			return (*this);
		}

		SettingsVariableBase(const SettingsVariableBase& svb)
		{
		}

		SettingsVariableBase()
		{
		}

	public:
		virtual void setString(const std::string& s) = 0;
		virtual std::string getString() = 0;
		virtual std::string isValid(const std::string& s) = 0;
		virtual void connectChangedBase(const boost::function<void(void)>& f, bool callnow = false) = 0;

		template <typename T>
		void setValue(const T& t)
		{
			std::string s;
			settingsmanager::tostring(t, s);
			setString(s);
		}

		template <typename T>
		T getValue()
		{
			T t;
			settingsmanager::fromstring(getString(), t);
			return t;
		}
};

template <typename T>
class SettingsVariable: public SettingsVariableBase
{
	private:
		T value;

	public:
		T get() const
		{
			boost::mutex::scoped_lock lock(mutex);
			return value;
		}

		bool operator==(const T& that) const
		{
			return get() == that;
		}

		bool operator>(const T& that) const
		{
			return get() > that;
		}

		bool operator>=(const T& that) const
		{
			return get() >= that;
		}

		bool operator<(const T& that) const
		{
			return get() < that;
		}

		bool operator<=(const T& that) const
		{
			return get() <= that;
		}

		bool operator!=(const T& that) const
		{
			return get() != that;
		}

		T operator+(const T& that) const
		{
			return get() + that;
		}

		T operator-(const T& that) const
		{
			return get() - that;
		}

		T operator-() const
		{
			return -get();
		}

		SettingsVariable<T>& operator+=(const T& that)
		{
			T t;
			{
				boost::mutex::scoped_lock lock(mutex);
				t = (value += that);
			}
			sigChanged(t);
			return *this;
		}

		SettingsVariable<T>& operator-=(const T& that)
		{
			T t;
			{
				boost::mutex::scoped_lock lock(mutex);
				t = (value -= that);
			}
			sigChanged(t);
			return *this;
		}

		void set(const T& t)
		{
			{
				boost::mutex::scoped_lock lock(mutex);
				if (value == t) return;
				value = t;
			}
			sigChanged(t);
		}

		boost::signals2::signal<void(const T& t)> sigChanged;

		void connectChanged(const typename boost::signals2::signal<void(const T& t)>::slot_function_type& f, bool callnow = false)
		{
			sigChanged.connect(f);
			if (callnow) f(get());
		}

		virtual void connectChangedBase(const boost::function<void(void)>& f, bool callnow = false)
		{
			sigChanged.connect(boost::bind(f));
			if (callnow) f();
		}

		SettingsVariable<T>& operator=(const T& t)
		{
			set(t);
			return *this;
		}

		SettingsVariable<T>& operator=(const SettingsVariable<T>& svt)
		{
			set(svt.get());
			return *this;
		}

		SettingsVariable<T>(const SettingsVariable<T>& svt)
		{
			set(svt.get());
		}

		SettingsVariable<T>()
		{
		}

		operator T()
		{
			return get();
		}

		virtual void setString(const std::string& s)
		{
			T t;
			settingsmanager::fromstring(s, t);
			set(t);
		}

		virtual std::string getString()
		{
			std::string s;
			settingsmanager::tostring(get(), s);
			return s;
		}

		virtual std::string isValid(const std::string& s)
		{
			T t;
			settingsmanager::fromstring(s, t);
			std::string o;
			settingsmanager::tostring(t, o);
			return o;
		}
};

class SettingsInfo {
	public:
		virtual std::string getDefault() = 0;

		virtual std::vector<std::string> getEnumStrings() = 0;
		virtual std::vector<std::string> getEnumValues() = 0;

		virtual std::string valueMin() = 0;
		virtual std::string valueMax() = 0;

		virtual bool isValid(const std::string& value) = 0;
};
typedef boost::shared_ptr<SettingsInfo> SettingsInfoRef;
SettingsInfoRef createSettingsInfoS(std::string def, std::string enum_str = "", std::string enum_val = "", std::string min = "", std::string max = "");

template <typename T>
SettingsInfoRef createSettingsInfo(T def, std::string enum_str = "", std::string enum_val = "", std::string min = "", std::string max = "")
{
	std::string s;
	settingsmanager::tostring(def, s);
	return createSettingsInfoS(s, enum_str, enum_val, min, max);
}

class SettingsManagerBase {
	protected:
		std::map<std::string, SettingsVariableBase*> m_curvalues;
		std::map<std::string, SettingsInfoRef> sinfo;

	public:
		SettingsInfoRef getInfo(const std::string& key);
		SettingsVariableBase* getVariable(const std::string& key);

		std::vector<std::string> getAllKeys();

		template <typename T>
		void registerSettingsVariableInfo(const std::string& key, SettingsVariable<T>& val, SettingsInfoRef sir)
		{
			m_curvalues[key] = &val;
			sinfo[key] = sir;
		}

		template <typename T, typename V>
		void registerSettingsVariable(const std::string& key, SettingsVariable<T>& val, const V& def, std::string enum_str = "", std::string enum_val = "", std::string min = "", std::string max = "")
		{
			registerSettingsVariableInfo<T>(key, val, createSettingsInfo<T>(def, enum_str, enum_val, min, max));
		}

		bool load(std::map<std::string, std::string>& values);
		bool load(const ext::filesystem::path& path);
		bool save(const ext::filesystem::path& path);

		virtual void fixLegacy(std::map<std::string, std::string>& values) = 0;

		void loadDefaults();
};

template<class S>
class SettingsManager: public SettingsManagerBase
{
	public:
		S s_curvalues;

		SettingsManager()
		{
			s_curvalues.registerSettings(this);
			loadDefaults();
		};

		virtual void fixLegacy(std::map<std::string, std::string>& values)
		{
			s_curvalues.fixLegacy(values);
		}
};

template<class S>
class SettingsManagerRef
{
	private:
		boost::shared_ptr<SettingsManager<S> > ref;

		SettingsManagerRef(SettingsManager<S>* s)
		: ref(s)
		{
		}

	public:
		static SettingsManagerRef<S> create()
		{
			return SettingsManagerRef<S>(new SettingsManager<S>());
		}

		boost::shared_ptr<SettingsManager<S> > get()
		{
			return ref;
		}

		S* operator->()
		{
			return &get()->s_curvalues;
		}

		S& operator*()
		{
			return get()->s_curvalues;
		}

		bool load()
		{
			return get()->load(get()->s_curvalues.getSavePath());
		}

		bool save()
		{
			return get()->save(get()->s_curvalues.getSavePath());
		}
};

#endif//SETTINGS_MANAGER_H
