// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/url_matcher.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_helpers.h"
#include "net/url_request/url_request.h"

namespace helpers = extensions::declarative_webrequest_helpers;
namespace keys = extensions::declarative_webrequest_constants;

namespace {
static extensions::URLMatcherConditionSet::ID g_next_id = 0;

// TODO(battre): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "experimental.webRequest.RequestMatcher";
const char kUnknownConditionAttribute[] = "Unknown condition attribute '%s'";
const char kConditionExpectedString[] =
    "Condition '%s' expected a string value";
const char kVectorOfStringsExpected[] =
    "Attribute '%s' expected a vector of strings";
const char kInvalidPortRanges[] = "Invalid port ranges";

// Registry for all factory methods of extensions::URLMatcherConditionFactory
// that allows translating string literals from the extension API into
// the corresponding factory method to be called.
class URLMatcherConditionFactoryMethods {
 public:
  URLMatcherConditionFactoryMethods() {
    typedef extensions::URLMatcherConditionFactory F;
    factory_methods_[keys::kHostContainsKey] = &F::CreateHostContainsCondition;
    factory_methods_[keys::kHostEqualsKey] = &F::CreateHostEqualsCondition;
    factory_methods_[keys::kHostPrefixKey] = &F::CreateHostPrefixCondition;
    factory_methods_[keys::kHostSuffixKey] = &F::CreateHostSuffixCondition;
    factory_methods_[keys::kPathContainsKey] = &F::CreatePathContainsCondition;
    factory_methods_[keys::kPathEqualsKey] = &F::CreatePathEqualsCondition;
    factory_methods_[keys::kPathPrefixKey] = &F::CreatePathPrefixCondition;
    factory_methods_[keys::kPathSuffixKey] = &F::CreatePathSuffixCondition;
    factory_methods_[keys::kQueryContainsKey] =
        &F::CreateQueryContainsCondition;
    factory_methods_[keys::kQueryEqualsKey] = &F::CreateQueryEqualsCondition;
    factory_methods_[keys::kQueryPrefixKey] = &F::CreateQueryPrefixCondition;
    factory_methods_[keys::kQuerySuffixKey] = &F::CreateQuerySuffixCondition;
    factory_methods_[keys::kURLContainsKey] = &F::CreateURLContainsCondition;
    factory_methods_[keys::kURLEqualsKey] = &F::CreateURLEqualsCondition;
    factory_methods_[keys::kURLPrefixKey] = &F::CreateURLPrefixCondition;
    factory_methods_[keys::kURLSuffixKey] = &F::CreateURLSuffixCondition;
  }

  // Returns whether a factory method for the specified |pattern_type| (e.g.
  // "host_suffix") is known.
  bool Contains(const std::string& pattern_type) const {
    return factory_methods_.find(pattern_type) != factory_methods_.end();
  }

  // Creates a URLMatcherCondition instance from |url_matcher_condition_factory|
  // of the given |pattern_type| (e.g. "host_suffix") for the given
  // |pattern_value| (e.g. "example.com").
  // The |pattern_type| needs to be known to this class (see Contains()) or
  // a CHECK is triggered.
  extensions::URLMatcherCondition Call(
      extensions::URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& pattern_type,
      const std::string& pattern_value) const {
    FactoryMethods::const_iterator i = factory_methods_.find(pattern_type);
    CHECK(i != factory_methods_.end());
    const FactoryMethod& method = i->second;
    return (url_matcher_condition_factory->*method)(pattern_value);
  }

 private:
  typedef extensions::URLMatcherCondition
      (extensions::URLMatcherConditionFactory::* FactoryMethod)
      (const std::string& prefix);
  typedef std::map<std::string, FactoryMethod> FactoryMethods;

  FactoryMethods factory_methods_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherConditionFactoryMethods);
};

static base::LazyInstance<URLMatcherConditionFactoryMethods>
    g_url_matcher_condition_factory_methods = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;

//
// WebRequestCondition
//

WebRequestCondition::WebRequestCondition(
    scoped_refptr<URLMatcherConditionSet> url_matcher_conditions,
    const WebRequestConditionAttributes& condition_attributes)
    : url_matcher_conditions_(url_matcher_conditions),
      condition_attributes_(condition_attributes),
      applicable_request_stages_(~0) {
  CHECK(url_matcher_conditions.get());
  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    applicable_request_stages_ &= (*i)->GetStages();
  }
}

WebRequestCondition::~WebRequestCondition() {}

bool WebRequestCondition::IsFulfilled(net::URLRequest* request,
                                      RequestStages request_stage) const {
  // All condition attributes must be fulfilled for a fulfilled condition.
  if (!(request_stage & applicable_request_stages_)) {
    // A condition that cannot be evaluated is considered as violated.
    return false;
  }

  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    if (!(*i)->IsFulfilled(request, request_stage))
      return false;
  }
  return true;
}

// static
scoped_ptr<WebRequestCondition> WebRequestCondition::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value& condition,
    std::string* error) {
  const base::DictionaryValue* condition_dict = NULL;
  if (!condition.GetAsDictionary(&condition_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<WebRequestCondition>(NULL);
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!condition_dict->GetString(keys::kInstanceTypeKey, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }
  if (instance_type != keys::kRequestMatcherType) {
    *error = kExpectedOtherConditionType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }

  WebRequestConditionAttributes attributes;
  URLMatcherConditionSet::Conditions url_matcher_conditions;
  scoped_ptr<URLMatcherSchemeFilter> url_matcher_schema_filter;
  scoped_ptr<URLMatcherPortFilter> url_matcher_port_filter;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       iter.HasNext(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const Value& condition_attribute_value = iter.value();
    if (condition_attribute_name == keys::kInstanceTypeKey) {
      // Skip this.
    } else if (IsURLMatcherConditionAttribute(condition_attribute_name)) {
      URLMatcherCondition url_matcher_condition =
          CreateURLMatcherCondition(
              url_matcher_condition_factory,
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
      url_matcher_conditions.insert(url_matcher_condition);
    } else if (condition_attribute_name == keys::kSchemesKey) {
      url_matcher_schema_filter = CreateURLMatcherScheme(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
    } else if (condition_attribute_name == keys::kPortsKey) {
      url_matcher_port_filter = CreateURLMatcherPorts(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
    } else if (WebRequestConditionAttribute::IsKnownType(
        condition_attribute_name)) {
      scoped_ptr<WebRequestConditionAttribute> attribute =
          WebRequestConditionAttribute::Create(
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
      attributes.push_back(make_linked_ptr(attribute.release()));
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  condition_attribute_name.c_str());
      return scoped_ptr<WebRequestCondition>(NULL);
    }
  }

  // As the URL is the preliminary matching criterion that triggers the tests
  // for the remaining condition attributes, we insert an empty URL match if
  // no other url match conditions were specified. Such an empty URL is always
  // matched.
  if (url_matcher_conditions.empty()) {
    url_matcher_conditions.insert(
        url_matcher_condition_factory->CreateHostPrefixCondition(""));
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set(
      new URLMatcherConditionSet(++g_next_id, url_matcher_conditions,
          url_matcher_schema_filter.Pass(), url_matcher_port_filter.Pass()));
  return scoped_ptr<WebRequestCondition>(
      new WebRequestCondition(url_matcher_condition_set, attributes));
}

// static
bool WebRequestCondition::IsURLMatcherConditionAttribute(
    const std::string& condition_attribute_name) {
  return g_url_matcher_condition_factory_methods.Get().Contains(
      condition_attribute_name);
}

// static
URLMatcherCondition WebRequestCondition::CreateURLMatcherCondition(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& condition_attribute_name,
    const base::Value* value,
    std::string* error) {
  std::string str_value;
  if (!value->GetAsString(&str_value)) {
    *error = base::StringPrintf(kConditionExpectedString,
                                condition_attribute_name.c_str());
    return URLMatcherCondition();
  }
  return g_url_matcher_condition_factory_methods.Get().Call(
      url_matcher_condition_factory, condition_attribute_name, str_value);
}

// static
scoped_ptr<URLMatcherSchemeFilter> WebRequestCondition::CreateURLMatcherScheme(
    const base::Value* value,
    std::string* error) {
  std::vector<std::string> schemas;
  if (!helpers::GetAsStringVector(value, &schemas)) {
    *error = base::StringPrintf(kVectorOfStringsExpected, keys::kSchemesKey);
    return scoped_ptr<URLMatcherSchemeFilter>(NULL);
  }
  return scoped_ptr<URLMatcherSchemeFilter>(
      new URLMatcherSchemeFilter(schemas));
}

// static
scoped_ptr<URLMatcherPortFilter> WebRequestCondition::CreateURLMatcherPorts(
    const base::Value* value,
    std::string* error) {
  std::vector<URLMatcherPortFilter::Range> ranges;
  const base::ListValue* value_list = NULL;
  if (!value->GetAsList(&value_list)) {
    *error = kInvalidPortRanges;
    return scoped_ptr<URLMatcherPortFilter>(NULL);
  }

  for (ListValue::const_iterator i = value_list->begin();
       i != value_list->end(); ++i) {
    Value* entry = *i;
    int port = 0;
    base::ListValue* range = NULL;
    if (entry->GetAsInteger(&port)) {
      ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    } else if (entry->GetAsList(&range)) {
      int from = 0, to = 0;
      if (range->GetSize() != 2u ||
          !range->GetInteger(0, &from) ||
          !range->GetInteger(1, &to)) {
        *error = kInvalidPortRanges;
        return scoped_ptr<URLMatcherPortFilter>(NULL);
      }
      ranges.push_back(URLMatcherPortFilter::CreateRange(from, to));
    } else {
      *error = kInvalidPortRanges;
      return scoped_ptr<URLMatcherPortFilter>(NULL);
    }
  }

  return scoped_ptr<URLMatcherPortFilter>(new URLMatcherPortFilter(ranges));
}

//
// WebRequestConditionSet
//

WebRequestConditionSet::WebRequestConditionSet(
    const std::vector<linked_ptr<WebRequestCondition> >& conditions)
    : conditions_(conditions) {
  for (Conditions::iterator i = conditions_.begin(); i != conditions_.end();
       ++i) {
    URLMatcherConditionSet::ID trigger_id =
        (*i)->url_matcher_condition_set_id();
    match_triggers_[trigger_id] = i->get();
  }
}

WebRequestConditionSet::~WebRequestConditionSet() {}

bool WebRequestConditionSet::IsFulfilled(
    URLMatcherConditionSet::ID url_match,
    net::URLRequest* request,
    RequestStages request_stage) const {
  MatchTriggers::const_iterator trigger = match_triggers_.find(url_match);
  DCHECK(trigger != match_triggers_.end());
  DCHECK_EQ(url_match, trigger->second->url_matcher_condition_set_id());
  return trigger->second->IsFulfilled(request, request_stage);
}

void WebRequestConditionSet::GetURLMatcherConditionSets(
    URLMatcherConditionSet::Vector* condition_sets) const {
  for (Conditions::const_iterator i = conditions_.begin();
       i != conditions_.end(); ++i) {
    condition_sets->push_back((*i)->url_matcher_condition_set());
  }
}

// static
scoped_ptr<WebRequestConditionSet> WebRequestConditionSet::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const AnyVector& conditions,
    std::string* error) {
  std::vector<linked_ptr<WebRequestCondition> > result;

  for (AnyVector::const_iterator i = conditions.begin();
       i != conditions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<WebRequestCondition> condition =
        WebRequestCondition::Create(url_matcher_condition_factory,
                                    (*i)->value(), error);
    if (!error->empty())
      return scoped_ptr<WebRequestConditionSet>(NULL);
    result.push_back(make_linked_ptr(condition.release()));
  }

  return scoped_ptr<WebRequestConditionSet>(new WebRequestConditionSet(result));
}

}  // namespace extensions
