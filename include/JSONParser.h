/*
 * JSONParser.h
 *
 *  Created on: 23.10.2012
 *      Author: tom
 */

#ifndef JSONPARSER_H_
#define JSONPARSER_H_

#include <QRect>
#include <QScriptEngine>
#include <QScriptValueIterator>

#include <cstdint>
#include <stdexcept>

#include "qt_helper.hxx"

#include <iostream>

class JSONNode
{
  public:
    explicit JSONNode(const QScriptValue& node = QScriptValue());
    explicit JSONNode(const QVariant& var);

    JSONNode const getChild(const QString& key) const;
    bool hasChild(const QString& key) const;

    template <class T>
    T getValue() const;

    template <class T>
    T getValue(const QString& key) const
    {
      return getChild(key).getValue<T>();
    }

    template <class T>
    T getValue(const QString& key, const T& def) const
    {
      if( !hasChild(key) )
        return def;
      return getValue<T>(key);
    }

  protected:
    QScriptValue  _node;
    QVariant      _var;
};
/**
 * Helper for handling json messages
 */
class JSONParser
{
  public:

    JSONParser(const QString& data);

    JSONNode const getChild(const QString& key) const;
    bool hasChild(const QString& key) const;

    template <class T>
    T getValue(const QString& key) const
    {
      return _root.getValue<T>(key);
    }

    template <class T>
    T getValue(const QString& key, const T& def) const
    {
      return _root.getValue<T>(key, def);
    }

  private:

    QScriptEngine   _engine;
    JSONNode        _root;
};

#endif /* JSONPARSER_H_ */
