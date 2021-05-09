/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_AST_IMPORTS_HPP
#define SASS_AST_IMPORTS_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "ast_nodes.hpp"
#include "ast_supports.hpp"
#include "stylesheet.hpp"

namespace Sass {

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  // Static imports are plain css imports with `url()`
  class StaticImport final : public ImportBase
  {
  private:

    // The URL for this import.
    // This already contains quotes.
    ADD_CONSTREF(InterpolationObj, url);

    // The supports condition attached to this import,
    // or `null` if no condition is attached.
    ADD_CONSTREF(SupportsConditionObj, supports);

    // The media query attached to this import,
    // or `null` if no condition is attached.
    ADD_CONSTREF(InterpolationObj, media);

    // Flag to hoist import to the top.
    ADD_CONSTREF(bool, outOfOrder);

  public:

    // Object constructor by values
    StaticImport(const SourceSpan& pstate,
      InterpolationObj url,
      SupportsConditionObj supports = {},
      InterpolationObj media = {});
    // Implement final up-casting method
    IMPLEMENT_ISA_CASTER(StaticImport);
  };
  // EO class StaticImport

  // Dynamic import beside its name must have a static url
  // We do not support to load sass partials programmatic
  // They also don't allow any supports or media queries.
  class IncludeImport final : public ImportBase, public ModRule
  {
  public:
    IncludeImport(const SourceSpan& pstate,
      const sass::string& prev,
      const sass::string& url,
      Import* import);
    // Implement final up-casting method
    IMPLEMENT_ISA_CASTER(IncludeImport);
  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}

#endif
