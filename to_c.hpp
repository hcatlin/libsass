#ifndef SASS_TO_C_H
#define SASS_TO_C_H

#include "operation.hpp"
#include "sass_values.h"

namespace Sass {
  using namespace std;

  class AST_Node;
  class Boolean;
  class Number;
  class Color;
  class String_Constant;
  class String_Quoted;
  class List;
  class Map;
  class Null;
  class Arguments;
  class Argument;

  class To_C : public Operation_CRTP<union Sass_Value*, To_C> {
    // import all the class-specific methods and override as desired
    using Operation<union Sass_Value*>::operator();
    // override this to define a catch-all
    union Sass_Value* fallback_impl(AST_Node* n);

  public:

    To_C() { };
    virtual ~To_C() { };

    union Sass_Value* operator()(Boolean*);
    union Sass_Value* operator()(Number*);
    union Sass_Value* operator()(Color*);
    union Sass_Value* operator()(String_Constant*);
    union Sass_Value* operator()(String_Quoted*);
    union Sass_Value* operator()(List*);
    union Sass_Value* operator()(Map*);
    union Sass_Value* operator()(Null*);
    union Sass_Value* operator()(Arguments*);
    union Sass_Value* operator()(Argument*);

    template <typename U>
    Sass_Value* fallback(U n) { return fallback_impl(n); }
  };

}

#endif
