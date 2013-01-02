#ifndef LR_COMPONENTARGUMENTS
#define LR_COMPONENTARGUMENTS

#include <configurable.h>
#include <common/hashedmap.h>

namespace LinksRouting
{

  class ComponentArguments:
    public virtual Configurable
  {
  protected:
    typedef void(*ComponentArgumentChangedCallbackBool)(const std::string& name, bool& val, void* arg);
    typedef void(*ComponentArgumentChangedCallbackInt)(const std::string& name, int& val, void* arg);
    typedef void(*ComponentArgumentChangedCallbackDouble)(const std::string& name, double& val, void* arg);
    typedef void(*ComponentArgumentChangedCallbackString)(const std::string& name, std::string& val, void* arg);

  private:
    struct ArgumentInfo
    {
      enum Type
      {
        ATBool, ATInt, ATDouble, ATString
      };
      const Type type;
      union
      {
        ComponentArgumentChangedCallbackBool cbBool;
        ComponentArgumentChangedCallbackInt cbInt;
        ComponentArgumentChangedCallbackDouble cbDouble;
        ComponentArgumentChangedCallbackString cbString;
      };
      void* cbArg;
      union
      {
        bool *valBool;
        int *valInt;
        double *valDouble;
        std::string *valString;
      };
      ArgumentInfo(bool& val, ComponentArgumentChangedCallbackBool cb = 0, void *cbA = 0) : type(ATBool),  cbBool(cb), cbArg(cbA), valBool(&val) { }
      ArgumentInfo(int& val, ComponentArgumentChangedCallbackInt cb, void *cbA = 0) : type(ATInt),  cbInt(cb), cbArg(cbA), valInt(&val) { }
      ArgumentInfo(double& val, ComponentArgumentChangedCallbackDouble cb, void *cbA = 0) : type(ATDouble),  cbDouble(cb), cbArg(cbA), valDouble(&val) { }
      ArgumentInfo(std::string& val, ComponentArgumentChangedCallbackString cb, void *cbA = 0) : type(ATString),  cbString(cb), cbArg(cbA), valString(&val) { }
    };
    typedef HashedMap<ArgumentInfo>::Type ArgumentMap;
    ArgumentMap argumentMap;

    ArgumentInfo* checkArgumentAvailability(const std::string& name, ArgumentInfo::Type type)
    {
      ArgumentMap::iterator found = argumentMap.find(name);
      if(found != argumentMap.end() && found->second.type == type)
        return &found->second;
      else
        return 0;
    }
  protected:

    ComponentArguments():
      Configurable("ComponentArguments")
    {}

    bool registerArg(const std::string& name, bool& val, ComponentArgumentChangedCallbackBool changeNotify = 0, void* cbArgument = 0)
    {
      if(argumentMap.find(name) != argumentMap.end()) return false;
      argumentMap.insert(std::make_pair(name, ArgumentInfo(val, changeNotify, cbArgument)));
      return true;
    }
    bool registerArg(const std::string& name, int& val, ComponentArgumentChangedCallbackInt changeNotify = 0, void* cbArgument = 0)
    {
      if(argumentMap.find(name) != argumentMap.end()) return false;
      argumentMap.insert(std::make_pair(name, ArgumentInfo(val, changeNotify, cbArgument)));
      return true;
    }
    bool registerArg(const std::string& name, double& val, ComponentArgumentChangedCallbackDouble changeNotify = 0, void* cbArgument = 0)
      {
      if(argumentMap.find(name) != argumentMap.end()) return false;
      argumentMap.insert(std::make_pair(name, ArgumentInfo(val, changeNotify, cbArgument)));
      return true;
    }
    bool registerArg(const std::string& name, std::string& val, ComponentArgumentChangedCallbackString changeNotify = 0, void* cbArgument = 0)
      {
      if(argumentMap.find(name) != argumentMap.end()) return false;
      argumentMap.insert(std::make_pair(name, ArgumentInfo(val, changeNotify, cbArgument)));
      return true;
    }
    bool unregisterArg(const std::string& name)
    {
      return argumentMap.erase(name) ? true : false;
    }

  public:
    virtual bool setFlag(const std::string& name, bool val)
    {
      ArgumentInfo* arg = checkArgumentAvailability(name, ArgumentInfo::ATBool);
      if(arg)
      {
        *arg->valBool = val;
        if(arg->cbBool) (*arg->cbBool)(name, *arg->valBool, arg->cbArg);
        return true;
      }
      return false;
    }

    virtual bool getFlag(const std::string& name, bool& val) const
    {
      ArgumentInfo* arg = const_cast<ComponentArguments*>(this)->checkArgumentAvailability(name, ArgumentInfo::ATBool);
      if(arg)
      {
        val = *arg->valBool;
        return true;
      }
      return false;
    }
    virtual bool setInteger(const std::string& name, int val)
    {
      ArgumentInfo* arg = checkArgumentAvailability(name, ArgumentInfo::ATInt);
      if(arg)
      {
        *arg->valInt = val;
        if(arg->cbInt) (*arg->cbInt)(name, *arg->valInt, arg->cbArg);
        return true;
      }
      return false;
    }
    virtual bool getInteger(const std::string& name, int& val) const
    {
      ArgumentInfo* arg = const_cast<ComponentArguments*>(this)->checkArgumentAvailability(name, ArgumentInfo::ATInt);
      if(arg)
      {
        val = *arg->valInt;
        return true;
      }
      return false;
    }
    virtual bool setFloat(const std::string& name, double val)
    {
      ArgumentInfo* arg = checkArgumentAvailability(name, ArgumentInfo::ATDouble);
      if(arg)
      {
        *arg->valDouble = val;
        if(arg->cbDouble) (*arg->cbDouble)(name, *arg->valDouble, arg->cbArg);
        return true;
      }
      return false;
    }
    virtual bool getFloat(const std::string& name, double& val) const
    {
      ArgumentInfo* arg = const_cast<ComponentArguments*>(this)->checkArgumentAvailability(name, ArgumentInfo::ATDouble);
      if(arg)
      {
        val = *arg->valDouble;
        return true;
      }
      return false;
    }
    virtual bool setString(const std::string& name, const std::string& val)
    {
      ArgumentInfo* arg = checkArgumentAvailability(name, ArgumentInfo::ATString);
      if(arg)
      {
        *arg->valString = val;
        if(arg->cbString) (*arg->cbString)(name, *arg->valString, arg->cbArg);
        return true;
      }
      return false;
    }
    virtual bool getString(const std::string& name, std::string& val) const
    {
      ArgumentInfo* arg = const_cast<ComponentArguments*>(this)->checkArgumentAvailability(name, ArgumentInfo::ATString);
      if(arg)
      {
        val = *arg->valString;
        return true;
      }
      return false;
    }
  };
};

#endif //LR_COMPONENT
