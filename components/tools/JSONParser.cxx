/*
 * JSONParser.cxx
 *
 *  Created on: Apr 9, 2013
 *      Author: tom
 */

#include "JSONParser.h"
#include "float2.hpp"

#include <QStringList>

//------------------------------------------------------------------------------
JSONNode::JSONNode(const QScriptValue& node):
  _node(node)
{
  if( _node.isVariant() )
    _var = _node.toVariant();
}

//------------------------------------------------------------------------------
JSONNode::JSONNode(const QVariant& var):
  _var( var )
{
  if( !var.isValid() )
    throw std::runtime_error("Invalid variant");
}

//------------------------------------------------------------------------------
JSONNode const JSONNode::getChild(const QString& key) const
{
  QScriptValue child = _node.property(key);

  if( !child.isValid() || child.isUndefined() )
    throw std::runtime_error("No such element (" + to_string(key) + ")");

  return JSONNode(child);
}

//------------------------------------------------------------------------------
bool JSONNode::hasChild(const QString& key) const
{
  QScriptValue prop = _node.property(key);
  return prop.isValid() && !prop.isUndefined();
}

//------------------------------------------------------------------------------
template<>
QString JSONNode::getValue() const
{
  if( !_node.isString() )
    throw std::runtime_error("Not a string");
  return _node.toString();
}

//------------------------------------------------------------------------------
template<>
std::string JSONNode::getValue() const
{
  return to_string(getValue<QString>());
}

//------------------------------------------------------------------------------
template<>
uint32_t JSONNode::getValue() const
{
  if( !_node.isNumber() )
    throw std::runtime_error("Not a number");
  return _node.toUInt32();
}

//------------------------------------------------------------------------------
template<>
QVariantList JSONNode::getValue() const
{
  if( _var.type() == QVariant::List )
    return _var.toList();

  if( !_node.isArray() )
    throw std::runtime_error("Not an array");

  return _node.toVariant().toList();
}

//------------------------------------------------------------------------------
template<>
QStringList JSONNode::getValue() const
{
  if( _var.type() == QVariant::StringList )
    return _var.toStringList();

  if( !_node.isArray() )
    throw std::runtime_error("Not an array");

  return _node.toVariant().toStringList();
}

//------------------------------------------------------------------------------
template<>
float2 JSONNode::getValue() const
{
  QVariantList region_list = getValue<QVariantList>();
  if( region_list.length() != 2 )
    throw std::runtime_error("Invalid float2 (length != 2)");

  return float2( region_list.at(0).toFloat(),
                 region_list.at(1).toFloat() );
}

//------------------------------------------------------------------------------
template<>
QPoint JSONNode::getValue() const
{
  return getValue<float2>().toQPoint();
}

//------------------------------------------------------------------------------
template<>
QRect JSONNode::getValue() const
{
  QVariantList region_list = getValue<QVariantList>();
  if( region_list.length() != 4 )
    throw std::runtime_error("Invalid region (length != 4)");

  return QRect( region_list.at(0).toInt(),
                region_list.at(1).toInt(),
                region_list.at(2).toInt(),
                region_list.at(3).toInt() );
}

//------------------------------------------------------------------------------
JSONParser::JSONParser(const QString& data)
{
  QScriptValue json = _engine.evaluate("("+data+")");

  if( !json.isValid() )
    throw std::runtime_error("Failed to evaluted received data.");

  if( _engine.hasUncaughtException() )
    throw std::runtime_error("Failed to evaluted received data: "
                             + to_string(json.toString()));

  if( !json.isArray() && !json.isObject() )
    throw std::runtime_error("Received data is no JSON object.");

  _root = JSONNode(json);
}

//------------------------------------------------------------------------------
JSONNode const JSONParser::getChild(const QString& key) const
{
  return _root.getChild(key);
}

//------------------------------------------------------------------------------
bool JSONParser::hasChild(const QString& key) const
{
  return _root.hasChild(key);
}

//----------------------------------------------------------------------------
JSONNode const JSONParser::getRoot() const
{
  return _root;
}
