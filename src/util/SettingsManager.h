#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <boost/thread.hpp>
#include <boost/signal.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/path.hpp>

#include <string>
#include <vector>
#include <map>

namespace settingsmanager {
	template <typename T>
	inline void fromstring(const std::string& in, T& out)
	{
		out = boost::lexical_cast<T>(in);
	}

	template <typename T>
	inline void tostring(const T& in, std::string& out)
	{
		out = boost::lexical_cast<std::string>(in);
	}
};

class SettingsVariableBase
{
	protected:
		mutable boost::mutex mutex;

		SettingsVariableBase& operator=(const SettingsVariableBase& svb)
		{
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

		boost::signal<void(const T& t)> sigChanged;

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

		template<typename T>
		void registerSettingsVariable(const std::string& key, SettingsVariable<T>& val, SettingsInfoRef sir)
		{
			m_curvalues[key] = &val;
			sinfo[key] = sir;
		}

		bool load(std::map<std::string, std::string>& values);
		bool load(const boost::filesystem::path& path);
		bool save(const boost::filesystem::path& path);

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
class SettingsManagerRef: public boost::shared_ptr<SettingsManager<S> >
{
	private:
		SettingsManagerRef(SettingsManager<S>* s)
		: boost::shared_ptr<SettingsManager<S> >(s)
		{
		}

	SettingsManager<S>* getpt() {
		return boost::shared_ptr<SettingsManager<S> >::get();
	}

	public:
		S* operator->()
		{
			return &getpt()->s_curvalues;
		}

		S& operator*()
		{
			return getpt()->s_curvalues;
		}

		static SettingsManagerRef create()
		{
			return SettingsManagerRef(new SettingsManager<S>());
		}

		bool load()
		{
			return getpt()->load(getpt()->s_curvalues.getSavePath());
		}

		bool save()
		{
			return getpt()->save(getpt()->s_curvalues.getSavePath());
		}

		void loadDefaults()
		{
			getpt()->loadDefaults();
		}
};

#endif//SETTINGS_MANAGER_H
