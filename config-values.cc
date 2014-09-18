/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
// Getting getopt_long()
#define _GNU_SOURCE 1

#include "config-values.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include <vector>

using std::vector;

// For simplicity, we keep the parameters all in a global list.
typedef vector<Parameter*> ParamList;
static ParamList *sRegisteredParameters = NULL;

Parameter::Parameter(const char *option_name_in,
                     char option_char_in, const char *helptext_in)
  : option_name(option_name_in),
    option_char(option_char_in), helptext(helptext_in) {
  if (sRegisteredParameters == NULL)
    sRegisteredParameters = new ParamList();
  sRegisteredParameters->push_back(this);
}

// Some specializations
template<> bool TypedParameter<std::string>::FromString(const char *s) {
  value_ = s;
  return true;
}
template<> std::string TypedParameter<std::string>::ToString() const {
  return value_;
}
template<> bool TypedParameter<std::string>::RequiresValue() const {
  return true;
}

template<> bool TypedParameter<float>::FromString(const char *s) {
  value_ = atof(s);
  return true;
}
template<> std::string TypedParameter<float>::ToString() const {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%.2f", value_);
  return buffer;
}
template<> bool TypedParameter<float>::RequiresValue() const { return true; }

template<> bool TypedParameter<int>::FromString(const char *s) {
  value_ = atoi(s);
  return true;
}
template<> std::string TypedParameter<int>::ToString() const {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%d", value_);
  return buffer;
}
template<> bool TypedParameter<int>::RequiresValue() const { return true; }

template<> bool TypedParameter<bool>::FromString(const char *s) {
  if (s == NULL || strlen(s) == 0) {
    value_ = !value_;    // no parameter: just toggling.
  } else {
    value_ = (strcasecmp(s, "on") == 0 || strcasecmp(s, "true") == 0);
  }
  return true;
}
template<> std::string TypedParameter<bool>::ToString() const {
  return value_ ? "on" : "off";
}
template<> bool TypedParameter<bool>::RequiresValue() const { return false; }

template<>
bool TypedParameter<std::pair<float,float> >::FromString(const char *s) {
  return sscanf(s, "%f,%f", &value_.first, &value_.second) == 2;
}

template<>
std::string TypedParameter<std::pair<float,float> >::ToString() const {
  char buffer[30];
  snprintf(buffer, sizeof(buffer), "%.2f,%.2f", value_.first, value_.second);
  return buffer;
}
template<> bool TypedParameter<std::pair<float,float> >::RequiresValue() const {
  return true;
}

int ParameterUsage(const char *progname) {
  fprintf(stderr, "usage: %s [options]\n", progname);
  if (!sRegisteredParameters)
    return 1;
  const int kIndentBetweenOptionAndHelp = 18;
  fprintf(stderr, "Synopsis:\n*** Long option%*s[short]: <help>\n",
          kIndentBetweenOptionAndHelp - 7, "");
  for (ParamList::const_iterator it = sRegisteredParameters->begin();
       it != sRegisteredParameters->end(); ++it) {
    int indent = kIndentBetweenOptionAndHelp;
    const Parameter *const p = *it;
    if (p->option_name == NULL && p->option_char == 0) {
      // This is a headline.
      fprintf(stderr, "\n [ %s ]\n", p->helptext);
      continue;
    }
    if (p->option_name) {
      fprintf(stderr, "    --%s <value> ",
              p->option_name);
      indent -= strlen(p->option_name);
    }
    if (p->option_char) indent -=4;
    fprintf(stderr, "%*s", indent, "");
    if (p->option_char) {
      fprintf(stderr, "[-%c]", p->option_char);
    }
    fprintf(stderr, ": %s (default: '%s')\n", p->helptext,
            p->ToString().c_str());
  }
  return 1;
}

bool SetParametersFromCommandline(int argc, char *argv[]) {
  if (sRegisteredParameters == NULL)
    return true;

  std::string optstring;
  struct option *long_options = new option [ sRegisteredParameters->size() + 1 ];
  int opt_idx = 0;
  for (size_t i = 0; i < sRegisteredParameters->size(); ++i) {
    const Parameter *const p = (*sRegisteredParameters)[i];
    if (p->option_name == NULL && p->option_char == 0) {
      continue;
    }
    if (p->option_char != 0) {
      optstring.append(1, p->option_char);
      if (p->RequiresValue()) optstring.append(1, ':');
    }
    struct option *opt = &long_options[opt_idx++];
    opt->name = p->option_name;
    opt->has_arg = p->RequiresValue() ? 1 : 2;
    opt->flag = NULL;
    opt->val = i + 256;
  }
  const char *optstr = optstring.c_str();
  bool success = true;
  int opt;
  int arg_idx = 0;
  while ((opt = getopt_long(argc, argv, optstr, long_options, &arg_idx)) >= 0) {
    if (opt == '?') {
      success = false;
      break;
    }
    for (size_t i = 0; i < sRegisteredParameters->size(); ++i) {
      Parameter *const p = (*sRegisteredParameters)[i];
      if (opt == p->option_char || opt == (int) (i + 256)) {
        p->FromString(optarg);
      }
    }
  }
  delete [] long_options;
  return success;
}
