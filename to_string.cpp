#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

#include "ast.hpp"
#include "inspect.hpp"
#include "context.hpp"
#include "to_string.hpp"

namespace Sass {
  using namespace std;

  To_String::To_String(Context* ctx)
  : ctx(ctx) { }
  To_String::~To_String() { }

  inline string To_String::fallback_impl(AST_Node* n)
  {

    OutputBuffer buffer;
    Emitter emitter(buffer, ctx);
    Inspect i(emitter, false);
    i.in_declaration_list = in_decl_list;
    n->perform(&i);
    return i.get_buffer();
  }

  inline string To_String::operator()(String_Constant* s)
  {
    return s->value();
  }

  inline string To_String::operator()(Null* n)
  { return ""; }
}
