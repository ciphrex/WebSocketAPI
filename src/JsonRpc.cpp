///////////////////////////////////////////////////////////////////////////////
//
// JsonRpc.cpp
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.

#include "JsonRpc.h"

using namespace JsonRpc;

void Request::setJson(const std::string& json)
{
    json_spirit::Value value;
    json_spirit::read_string(json, value);
    if (value.type() != json_spirit::obj_type)
    {
        throw std::runtime_error("Invalid JSON.");
    }

    const json_spirit::Object& obj = value.get_obj();
    const json_spirit::Value& method = json_spirit::find_value(obj, "method");
    if (method.type() != json_spirit::str_type)
    {
        throw std::runtime_error("Missing method.");
    }

    const json_spirit::Value& params = json_spirit::find_value(obj, "params");
    if (params.is_null())
    {
        m_params = json_spirit::Array();
    }
    else if (params.type() != json_spirit::array_type)
    {
        throw std::runtime_error("Invalid parameter format.");
    }
    else
    {
        m_params = params.get_array();
    }

    m_method = method.get_str();
    m_id = json_spirit::find_value(obj, "id");
}

std::string Request::getJson() const
{
    json_spirit::Object req;
    req.push_back(json_spirit::Pair("method", m_method));
    req.push_back(json_spirit::Pair("params", m_params));
    req.push_back(json_spirit::Pair("id", m_id));
    return json_spirit::write_string<json_spirit::Value>(req);
}



void Response::setJson(const std::string& json)
{
    json_spirit::Value value;
    json_spirit::read_string(json, value);
    if (value.type() != json_spirit::obj_type) {
        throw std::runtime_error("Invalid JSON.");
    }
    const json_spirit::Object& obj = value.get_obj();
    m_result = json_spirit::find_value(obj, "result");
    m_error = json_spirit::find_value(obj, "error");
    m_id = json_spirit::find_value(obj, "id");
}

std::string Response::getJson() const
{
    json_spirit::Object res;
    res.push_back(json_spirit::Pair("result", m_result));
    res.push_back(json_spirit::Pair("error", m_error));
    res.push_back(json_spirit::Pair("id", m_id));
    return json_spirit::write_string<json_spirit::Value>(res);
}

void Response::setResult(const json_spirit::Value& result, const json_spirit::Value& id)
{
    m_result = result;
    m_error = json_spirit::Value();
    m_id = id;
}

void Response::setError(const json_spirit::Value& error, const json_spirit::Value& id)
{
    m_result = json_spirit::Value();
    m_error = error;
    m_id = id;
}

