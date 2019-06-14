// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "extender.hpp"
#include "permutate.hpp"
#include "dart_helpers.hpp"

namespace Sass {

  bool hasExactlyOne(ComplexSelectorObj vec)
  {
    return vec->length() == 1;
  }

  bool hasMoreThanOne(ComplexSelectorObj vec)
  {
    return vec->length() > 1;
  }

  Extender::Extender(Backtraces& traces) :
    mode(NORMAL),
    traces(traces),
    selectors(),
    extensions(),
    extensionsByExtender(),
    mediaContexts(),
    sourceSpecificity(),
    originals()
  {}

  Extender::Extender(ExtendMode mode, Backtraces& traces) :
    mode(mode),
    traces(traces),
    selectors(),
    extensions(),
    extensionsByExtender(),
    mediaContexts(),
    sourceSpecificity(),
    originals()
  {}

  ExtSmplSelSet Extender::getSimpleSelectors() const
  {
    ExtSmplSelSet set;
    for (auto entry : selectors) {
      set.insert(entry.first);
    }
    return set;
  }

  // ##########################################################################
  // Extends [selector] with [source] extender and [targets] extendees.
  // This works as though `source {@extend target}` were written in the
  // stylesheet, with the exception that [target] can contain compound
  // selectors which must be extended as a unit.
  // ##########################################################################
  SelectorListObj Extender::extend(
    SelectorListObj selector,
    SelectorListObj source,
    SelectorListObj targets,
    Backtraces& traces)
  {
    return extendOrReplace(selector, source, targets, ExtendMode::TARGETS, traces);
  }
  // EO Extender::extend

  // ##########################################################################
  // Returns a copy of [selector] with [targets] replaced by [source].
  // ##########################################################################
  SelectorListObj Extender::replace(
    SelectorListObj selector,
    SelectorListObj source,
    SelectorListObj targets,
    Backtraces& traces)
  {
    return extendOrReplace(selector, source, targets, ExtendMode::REPLACE, traces);
  }
  // EO Extender::replace

  // ##########################################################################
  // A helper function for [extend] and [replace].
  // ##########################################################################
  SelectorListObj Extender::extendOrReplace(
    SelectorListObj selector,
    SelectorListObj source,
    SelectorListObj targets,
    ExtendMode mode,
    Backtraces& traces)
  {
    ExtSelExtMapEntry extenders;

    for (auto complex : source->elements()) {
      // Extension.oneOff(complex as ComplexSelector)
      extenders.insert(complex, Extension(complex));
    }

    for (auto complex : targets->elements()) {

      if (complex->length() != 1) {
        // throw "can't extend complex selector $complex."
      }

      if (auto compound = complex->first()->getCompound()) {

        ExtSelExtMap extensions;

        for (auto simple : compound->elements()) {
          extensions.insert(std::make_pair(simple, extenders));
        }

        Extender extender(mode, traces);

        if (!selector->is_invisible()) {
          for (auto sel : selector->elements()) {
            extender.originals.insert(sel);
          }
        }

        selector = extender.extendList(selector, extensions, {});

      }

    }

    return selector;

  }
  // EO extendOrReplace

  // ##########################################################################
  // Adds [selector] to this extender, with [selectorSpan] as the span covering
  // the selector and [ruleSpan] as the span covering the entire style rule.
  // Extends [selector] using any registered extensions, then returns an empty
  // [ModifiableCssStyleRule] with the resulting selector. If any more relevant
  // extensions are added, the returned rule is automatically updated.
  // The [mediaContext] is the media query context in which the selector was
  // defined, or `null` if it was defined at the top level of the document.
  // ##########################################################################
  void Extender::addSelector(SelectorListObj selector, CssMediaRule_Obj mediaContext)
  {

    SelectorListObj original = selector;
    if (!original->isInvisible()) {
      for (auto complex : selector->elements()) {
        originals.insert(complex);
      }
    }

    if (!extensions.empty()) {

      SelectorListObj res = extendList(original, extensions, mediaContext);

      selector->elements(res->elements());

    }

    if (!mediaContext.isNull()) {
      mediaContexts.insert(selector, mediaContext);
    }

    registerSelector(selector, selector);

  }
  // EO addSelector

  // ##########################################################################
  // Registers the [SimpleSelector]s in [list]
  // to point to [rule] in [selectors].
  // ##########################################################################
  void Extender::registerSelector(SelectorListObj list, SelectorListObj rule)
  {
    if (list.isNull() || list->empty()) return;
    for (auto complex : list->elements()) {
      for (auto component : complex->elements()) {
        if (auto compound = component->getCompound()) {
          for (SimpleSelector* simple : compound->elements()) {
            selectors[simple].insert(rule);
            if (auto pseudo = simple->getPseudoSelector()) {
              if (pseudo->selector()) {
                registerSelector(pseudo->selector(), rule);
              }
            }
          }
        }
      }
    }
  }
  // EO registerSelector

  // ##########################################################################
  // Returns an extension that combines [left] and [right]. Throws 
  // a [SassException] if [left] and [right] have incompatible 
  // media contexts. Throws an [ArgumentError] if [left]
  // and [right] don't have the same extender and target.
  // ##########################################################################
  Extension mergeExtension(Extension lhs, Extension rhs)
  {
    // If one extension is optional and doesn't add a
    // special media context, it doesn't need to be merged.
    if (rhs.isOptional && rhs.mediaContext.isNull()) return lhs;
    if (lhs.isOptional && lhs.mediaContext.isNull()) return rhs;

    Extension rv(lhs);
    // ToDo: is this right?
    rv.isOptional = true;
    rv.isOriginal = false;
    return rv;
  }
  // EO mergeExtension

  // ##########################################################################
  // Helper function to copy extension between maps
  // ##########################################################################
  void mapCopyExts(ExtSelExtMap& dest, ExtSelExtMap& source)
  {
    for (auto it : source) {
      SimpleSelectorObj key = it.first;
      ExtSelExtMapEntry& inner = it.second;
      ExtSelExtMap::iterator dmap = dest.find(key);
      if (dmap == dest.end()) {
        dest.insert(std::make_pair(key, inner));
      }
      else {
        // ToDo: optimize ordered_map API!
        for (ComplexSelectorObj it2 : inner) {
          ExtSelExtMapEntry& imap = dest[key];
          imap.insert(it2, inner.get(it2));
        }
      }
    }
  }

  // ##########################################################################
  // Adds an extension to this extender. The [extender] is the selector for the
  // style rule in which the extension is defined, and [target] is the selector
  // passed to `@extend`. The [extend] provides the extend span and indicates 
  // whether the extension is optional. The [mediaContext] defines the media query
  // context in which the extension is defined. It can only extend selectors
  // within the same context. A `null` context indicates no media queries.
  // ##########################################################################
  // ToDo: rename extender to parent, since it is not involved in extending stuff
  // ToDo: check why dart sass passes the ExtendRule around (is this the main selector?)
  // ##########################################################################
  // Note: this function could need some logic cleanup
  // ##########################################################################
  void Extender::addExtension(
    SelectorListObj extender,
    SimpleSelectorObj target,
    ExtendRule_Obj extend,
    CssMediaRule_Obj mediaQueryContext)
  {

    auto rules = selectors.find(target);
    bool hasRule = rules != selectors.end();

    ExtSelExtMapEntry newExtensions;

    auto existingExtensions = extensionsByExtender.find(target);
    bool hasExistingExtensions = existingExtensions != extensionsByExtender.end();

    ExtSelExtMapEntry& sources = extensions[target];

    for (auto complex : extender->elements()) {

      Extension state(complex);
      // ToDo: fine-tune public API
      state.target = target;
      state.isOptional = extend->isOptional();
      state.mediaContext = mediaQueryContext;

      if (sources.hasKey(complex)) {
        // If there's already an extend from [extender] to [target],
        // we don't need to re-run the extension. We may need to
        // mark the extension as mandatory, though.
        // sources.insert(complex, mergeExtension(existingState->second, state);
        // ToDo: implement behavior once use case is found!?
        continue;
      }

      sources.insert(complex, state);

      for (auto component : complex->elements()) {
        if (auto compound = component->getCompound()) {
          for (auto simple : compound->elements()) {
            extensionsByExtender[simple].push_back(state);
            if (sourceSpecificity.find(simple) == sourceSpecificity.end()) {
              // Only source specificity for the original selector is relevant.
              // Selectors generated by `@extend` don't get new specificity.
              sourceSpecificity[simple] = complex->maxSpecificity();
            }
          }
        }
      }

      if (hasRule || hasExistingExtensions) {
        newExtensions.insert(complex, state);
      }

    }
    // EO foreach complex

    if (newExtensions.empty()) {
      return;
    }

    ExtSelExtMap newExtensionsByTarget;
    newExtensionsByTarget.insert(std::make_pair(target, newExtensions));
    existingExtensions = extensionsByExtender.find(target);
    if (hasExistingExtensions && !existingExtensions->second.empty()) {
      auto additionalExtensions =
        extendExistingExtensions(existingExtensions->second, newExtensionsByTarget);
      if (!additionalExtensions.empty()) {
        mapCopyExts(newExtensionsByTarget, additionalExtensions);
      }
    }

    if (hasRule) {
      extendExistingStyleRules(selectors[target], newExtensionsByTarget);
    }

  }
  // EO addExtension
  
  // ##########################################################################
  // Extend [extensions] using [newExtensions].
  // ##########################################################################
  // Note: dart-sass throws an error in here
  // ##########################################################################
  void Extender::extendExistingStyleRules(
    ExtListSelSet& rules,
    ExtSelExtMap& newExtensions)
  {
    // Is a modifyableCssStyleRUle in dart sass
    for (SelectorListObj rule : rules) {
      SelectorListObj oldValue = SASS_MEMORY_COPY(rule);
      CssMediaRule_Obj mediaContext;
      if (mediaContexts.hasKey(rule)) mediaContext = mediaContexts.get(rule);
      SelectorListObj ext = extendList(rule, newExtensions, mediaContext);
      // If no extends actually happenedit (for example becaues unification
      // failed), we don't need to re-register the selector.
      if (ObjEqualityFn(oldValue, ext)) continue;
      rule->elements(ext->elements());
      registerSelector(rule, rule);

    }
  }
  // EO extendExistingStyleRules

  // ##########################################################################
  // Extend [extensions] using [newExtensions]. Note that this does duplicate
  // some work done by [_extendExistingStyleRules],  but it's necessary to
  // expand each extension's extender separately without reference to the full
  // selector list, so that relevant results don't get trimmed too early.
  //
  // Returns extensions that should be added to [newExtensions] before
  // extending selectors in order to properly handle extension loops such as:
  //
  //     .c {x: y; @extend .a}
  //     .x.y.a {@extend .b}
  //     .z.b {@extend .c}
  //
  // Returns `null` (Note: empty map) if there are no extensions to add.
  // ##########################################################################
  // Note: maybe refactor to return `bool` (and pass reference)
  // Note: dart-sass throws an error in here
  // ##########################################################################
  ExtSelExtMap Extender::extendExistingExtensions(
    std::vector<Extension> oldExtensions,
    ExtSelExtMap& newExtensions)
  {

    ExtSelExtMap additionalExtensions;

    for (Extension extension : oldExtensions) {
      ExtSelExtMapEntry& sources = extensions[extension.target];
      std::vector<ComplexSelectorObj> selectors;

      selectors = extendComplex(
        extension.extender,
        newExtensions,
        extension.mediaContext
      );

      if (selectors.empty()) {
        continue;
      }

      // ToDo: "catch" error from extend

      bool first = false;
      bool containsExtension = ObjEqualityFn(selectors.front(), extension.extender);
      for (ComplexSelectorObj complex : selectors) {
        // If the output contains the original complex 
        // selector, there's no need to recreate it.
        if (containsExtension && first) {
          first = false;
          continue;
        }

        Extension withExtender = extension.withExtender(complex);
        if (sources.hasKey(complex)) {
          sources.insert(complex, mergeExtension(
            sources.get(complex), withExtender));
        }
        else {
          sources.insert(complex, withExtender);
          for (auto component : complex->elements()) {
            if (auto compound = component->getCompound()) {
              for (auto simple : compound->elements()) {
                extensionsByExtender[simple].push_back(withExtender);
              }
            }
          }
          if (newExtensions.find(extension.target) != newExtensions.end()) {
            additionalExtensions[extension.target].insert(complex, withExtender);
          }
        }
      }

      // If [selectors] doesn't contain [extension.extender],
      // for example if it was replaced due to :not() expansion,
      // we must get rid of the old version.
      if (!containsExtension) {
        sources.erase(extension.extender);
      }

    }

    return additionalExtensions;

  }
  // EO extendExistingExtensions

  // ##########################################################################
  // Extends [list] using [extensions].
  // ##########################################################################
  SelectorListObj Extender::extendList(
    SelectorListObj list,
    ExtSelExtMap& extensions,
    CssMediaRule_Obj mediaQueryContext)
  {

    // This could be written more simply using [List.map], but we want to
    // avoid any allocations in the common case where no extends apply.
    std::vector<ComplexSelectorObj> extended;
    for (size_t i = 0; i < list->length(); i++) {
      ComplexSelectorObj complex = list->get(i);
      std::vector<ComplexSelectorObj> result =
        extendComplex(complex, extensions, mediaQueryContext);
      if (result.empty()) {
        if (!extended.empty()) {
          extended.push_back(complex);
        }
      }
      else {
        if (extended.empty()) {
          for (size_t n = 0; n < i; n += 1) {
            extended.push_back(list->get(n));
          }
        }
        for (auto sel : result) {
          extended.push_back(sel);
        }
      }
    }

    if (extended.empty()) {
      return list;
    }

    SelectorListObj rv = SASS_MEMORY_NEW(SelectorList, list->pstate());
    return rv->concat(trim(extended, originals));

  }
  // EO extendList

  // ##########################################################################
  // Extends [complex] using [extensions], and
  // returns the contents of a [SelectorList].
  // ##########################################################################
  std::vector<ComplexSelectorObj> Extender::extendComplex(
    ComplexSelectorObj complex,
    ExtSelExtMap& extensions,
    CssMediaRule_Obj mediaQueryContext)
  {

    // The complex selectors that each compound selector in [complex.components]
    // can expand to.
    //
    // For example, given
    //
    //     .a .b {...}
    //     .x .y {@extend .b}
    //
    // this will contain
    //
    //     [
    //       [.a],
    //       [.b, .x .y]
    //     ]
    //
    // This could be written more simply using [List.map], but we want to avoid
    // any allocations in the common case where no extends apply.

    std::vector<ComplexSelectorObj> result;
    std::vector<std::vector<ComplexSelectorObj>> extendedNotExpanded;
    bool isOriginal = originals.find(complex) != originals.end();
    for (size_t i = 0; i < complex->length(); i += 1) {
      SelectorComponentObj component = complex->get(i);
      if (CompoundSelectorObj compound = Cast<CompoundSelector>(component)) {
        std::vector<ComplexSelectorObj> extended = extendCompound(compound, extensions, mediaQueryContext, isOriginal);
        if (extended.empty()) {
          if (!extendedNotExpanded.empty()) {
            extendedNotExpanded.push_back({
              compound->wrapInComplex()
            });
          }
        }
        else {
          // Note: dart-sass checks for null!?
          if (extendedNotExpanded.empty()) {
            for (size_t n = 0; n < i; n++) {
                extendedNotExpanded.push_back({
                  complex->at(n)->wrapInComplex()
                });
            }
          }
          extendedNotExpanded.push_back(extended);
        }
      }
      else {
        // Note: dart-sass checks for null!?
        if (!extendedNotExpanded.empty()) {
          extendedNotExpanded.push_back({
            component->wrapInComplex()
          });
        }
      }
    }

    // Note: dart-sass checks for null!?
    if (extendedNotExpanded.empty()) {
      return {};
    }

    bool first = true;

    // ToDo: either change weave or paths to work with the same data?
    std::vector<std::vector<ComplexSelectorObj>>
      paths = permutate(extendedNotExpanded);
    
    for (std::vector<ComplexSelectorObj> path : paths) {
      // Unpack the inner complex selector to component list
      std::vector<std::vector<SelectorComponentObj>> _paths;
      for (ComplexSelectorObj sel : path) {
        _paths.insert(_paths.end(), sel->elements());
      }

      std::vector<std::vector<SelectorComponentObj>> weaved = weave(_paths);

      for (std::vector<SelectorComponentObj> components : weaved) {

        ComplexSelectorObj cplx = SASS_MEMORY_NEW(ComplexSelector, "[phony]");
        cplx->hasPreLineFeed(complex->hasPreLineFeed());
        for (auto pp : path) {
          if (pp->hasPreLineFeed()) {
            cplx->hasPreLineFeed(true);
          }
        }
        cplx->elements(components);

        // Make sure that copies of [complex] retain their status
        // as "original" selectors. This includes selectors that
        // are modified because a :not() was extended into.
        if (first && originals.find(complex) != originals.end()) {
          originals.insert(cplx);
        }
        first = false;

        result.push_back(cplx);

      }

    }

    return result;
  }
  // EO extendComplex

  // ##########################################################################
  // Returns a one-off [Extension] whose extender
  // is composed solely of [simple].
  // ##########################################################################
  Extension Extender::extensionForSimple(SimpleSelectorObj simple)
  {
    Extension extension(simple->wrapInComplex());
    extension.specificity = maxSourceSpecificity(simple);
    extension.isOriginal = true;
    return extension;
  }
  // Extender::extensionForSimple

  // ##########################################################################
  // Returns a one-off [Extension] whose extender is composed
  // solely of a compound selector containing [simples].
  // ##########################################################################
  Extension Extender::extensionForCompound(std::vector<SimpleSelectorObj> simples)
  {
    CompoundSelectorObj compound = SASS_MEMORY_NEW(CompoundSelector, ParserState("[ext]"));
    Extension extension(compound->concat(simples)->wrapInComplex());
    // extension.specificity = sourceSpecificity[simple];
    extension.isOriginal = true;
    return extension;
  }
  // EO extensionForCompound

  // ##########################################################################
  // Extends [compound] using [extensions], and returns the
  // contents of a [SelectorList]. The [inOriginal] parameter
  // indicates whether this is in an original complex selector,
  // meaning that [compound] should not be trimmed out.
  // ##########################################################################
  std::vector<ComplexSelectorObj> Extender::extendCompound(
    CompoundSelectorObj compound,
    ExtSelExtMap& extensions,
    CssMediaRule_Obj mediaQueryContext,
    bool inOriginal)
  {

    // If there's more than one target and they all need to
    // match, we track which targets are actually extended.
    ExtSmplSelSet targetsUsed2;

    ExtSmplSelSet* targetsUsed = nullptr;

    if (mode != ExtendMode::NORMAL && extensions.size() > 1) {
      targetsUsed = &targetsUsed2;
    }

    std::vector<ComplexSelectorObj> result;
    // The complex selectors produced from each component of [compound].
    std::vector<std::vector<Extension>> options;

    for (size_t i = 0; i < compound->length(); i++) {
      SimpleSelectorObj simple = compound->get(i);
      auto extended = extendSimple(simple, extensions, mediaQueryContext, targetsUsed);
      if (extended.empty()) {
        if (!options.empty()) {
          options.push_back({ extensionForSimple(simple) });
        }
      }
      else {
        if (options.empty()) {
          if (i != 0) {
            std::vector<SimpleSelectorObj> in;
            for (size_t n = 0; n < i; n += 1) {
              in.push_back(compound->get(n));
            }
            options.push_back({ extensionForCompound(in) });
          }
        }
        options.insert(options.end(),
          extended.begin(), extended.end());
      }
    }

    if (options.empty()) {
      return {};
    }

    // If [_mode] isn't [ExtendMode.normal] and we didn't use all
    // the targets in [extensions], extension fails for [compound].
    if (targetsUsed != nullptr) {

      if (targetsUsed->size() != extensions.size()) {
        if (!targetsUsed->empty()) {
          return {};
        }
      }
    }

    // Optimize for the simple case of a single simple
    // selector that doesn't need any unification.
    if (options.size() == 1) {
      std::vector<Extension> exts = options[0];
      for (size_t n = 0; n < exts.size(); n += 1) {
        exts[n].assertCompatibleMediaContext(mediaQueryContext, traces);
        result.push_back(exts[n].extender);
      }
      return result;
    }

    // Find all paths through [options]. In this case, each path represents a
    // different unification of the base selector. For example, if we have:
    //
    //     .a.b {...}
    //     .w .x {@extend .a}
    //     .y .z {@extend .b}
    //
    // then [options] is `[[.a, .w .x], [.b, .y .z]]` and `paths(options)` is
    //
    //     [
    //       [.a, .b],
    //       [.a, .y .z],
    //       [.w .x, .b],
    //       [.w .x, .y .z]
    //     ]
    //
    // We then unify each path to get a list of complex selectors:
    //
    //     [
    //       [.a.b],
    //       [.y .a.z],
    //       [.w .x.b],
    //       [.w .y .x.z, .y .w .x.z]
    //     ]

    bool first = mode != ExtendMode::REPLACE;
    std::vector<ComplexSelectorObj> unifiedPaths;
    std::vector<std::vector<Extension>> prePaths = permutate(options);

    for (size_t i = 0; i < prePaths.size(); i += 1) {
      std::vector<std::vector<SelectorComponentObj>> complexes;
      std::vector<Extension> path = prePaths.at(i);
      if (first) {
        // The first path is always the original selector. We can't just
        // return [compound] directly because pseudo selectors may be
        // modified, but we don't have to do any unification.
        first = false;
        CompoundSelectorObj mergedSelector =
          SASS_MEMORY_NEW(CompoundSelector, "[ext]");
        for (size_t n = 0; n < path.size(); n += 1) {
          ComplexSelectorObj sel = path[n].extender;
          if (CompoundSelectorObj compound = Cast<CompoundSelector>(sel->last())) {
            mergedSelector->concat(compound->elements());
          }
        }
        complexes.push_back({ mergedSelector });
      }
      else {
        std::vector<SimpleSelectorObj> originals;
        std::vector<std::vector<SelectorComponentObj>> toUnify;

        for (auto state : path) {
          if (state.isOriginal) {
            ComplexSelectorObj sel = state.extender;
            if (CompoundSelectorObj compound = Cast<CompoundSelector>(sel->last())) {
              originals.insert(originals.end(), compound->last());
            }
          }
          else {
            toUnify.push_back(state.extender->elements());
          }
        }
        if (!originals.empty()) {
          CompoundSelectorObj merged =
            SASS_MEMORY_NEW(CompoundSelector, "[phony]");
          toUnify.insert(toUnify.begin(), { merged->concat(originals) });
        }
        complexes = unifyComplex(toUnify);
        if (complexes.empty()) {
          return {};
        }

      }

      bool lineBreak = false;
      // var specificity = _sourceSpecificityFor(compound);
      for (auto state : path) {
        state.assertCompatibleMediaContext(mediaQueryContext, traces);
        lineBreak = lineBreak || state.extender->hasPreLineFeed();
        // specificity = math.max(specificity, state.specificity);
      }

      for (std::vector<SelectorComponentObj> components : complexes) {
        auto sel = SASS_MEMORY_NEW(ComplexSelector, "[ext]");
        sel->hasPreLineFeed(lineBreak);
        sel->elements(components);
        unifiedPaths.push_back(sel);
      }

    }

    return unifiedPaths;
  }
  // EO extendCompound

  // ##########################################################################
  // Extends [simple] without extending the
  // contents of any selector pseudos it contains.
  // ##########################################################################
  std::vector<Extension> Extender::extendWithoutPseudo(
    SimpleSelectorObj simple,
    ExtSelExtMap& extensions,
    ExtSmplSelSet* targetsUsed)
  {

    auto extension = extensions.find(simple);
    if (extension == extensions.end()) return {};
    ExtSelExtMapEntry& extenders = extension->second;

    if (targetsUsed != nullptr) {
      targetsUsed->insert(simple);
    }
    if (mode == ExtendMode::REPLACE) {
      return extenders.values();
    }

    extenders = extensions[simple];
    const std::vector<Extension>&
      values = extenders.values();
    std::vector<Extension> result;
    result.reserve(values.size() + 1);
    result.push_back(extensionForSimple(simple));
    result.insert(result.end(), values.begin(), values.end());
    return result;
  }
  // EO extendWithoutPseudo

  // ##########################################################################
  // Extends [simple] and also extending the
  // contents of any selector pseudos it contains.
  // ##########################################################################
  std::vector<std::vector<Extension>> Extender::extendSimple(
    SimpleSelectorObj simple,
    ExtSelExtMap& extensions,
    CssMediaRule_Obj mediaQueryContext,
    ExtSmplSelSet* targetsUsed)
  {
    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(simple)) {
      if (pseudo->selector()) {
        std::vector<std::vector<Extension>> merged;
        std::vector<Pseudo_Selector_Obj> extended =
          extendPseudo(pseudo, extensions, mediaQueryContext);
        for (auto extend : extended) {
          std::vector<Extension> result =
            extendWithoutPseudo(extend, extensions, targetsUsed);
          if (result.empty()) result = { extensionForSimple(extend) };
          merged.push_back(result);
        }
        if (!extended.empty()) {
          return merged;
        }
      }
    }
    std::vector<Extension> result =
      extendWithoutPseudo(simple, extensions, targetsUsed);
    if (result.empty()) return {};
    return { result };
  }
  // extendSimple

  // ##########################################################################
  // Inner loop helper for [extendPseudo] function
  // ##########################################################################
  std::vector<ComplexSelectorObj> extendPseudoComplex(
    ComplexSelectorObj complex,
    Pseudo_Selector_Obj pseudo,
    CssMediaRule_Obj mediaQueryContext)
  {

    if (complex->length() != 1) { return { complex }; }
    auto compound = Cast<CompoundSelector>(complex->get(0));
    if (compound == nullptr) { return { complex }; }
    if (compound->length() != 1) { return { complex }; }
    auto innerPseudo = Cast<Pseudo_Selector>(compound->get(0));
    if (innerPseudo == nullptr) { return { complex }; }
    if (!innerPseudo->selector()) { return { complex }; }

    std::string name(pseudo->normalized());

    if (name == "not") {
      // In theory, if there's a `:not` nested within another `:not`, the
      // inner `:not`'s contents should be unified with the return value.
      // For example, if `:not(.foo)` extends `.bar`, `:not(.bar)` should
      // become `.foo:not(.bar)`. However, this is a narrow edge case and
      // supporting it properly would make this code and the code calling it
      // a lot more complicated, so it's not supported for now.
      if (innerPseudo->normalized() != "matches") return {};
      return innerPseudo->selector()->elements();
    }
    else if (name == "matches" && name == "any" && name == "current" && name == "nth-child" && name == "nth-last-child") {
      // As above, we could theoretically support :not within :matches, but
      // doing so would require this method and its callers to handle much
      // more complex cases that likely aren't worth the pain.
      if (innerPseudo->name() != pseudo->name()) return {};
      if (!ObjEquality()(innerPseudo->argument(), pseudo->argument())) return {};
      return innerPseudo->selector()->elements();
    }
    else if (name == "has" && name == "host" && name == "host-context" && name == "slotted") {
      // We can't expand nested selectors here, because each layer adds an
      // additional layer of semantics. For example, `:has(:has(img))`
      // doesn't match `<div><img></div>` but `:has(img)` does.
      return { complex };
    }

    return {};

  }
  // EO extendPseudoComplex

  // ##########################################################################
  // Extends [pseudo] using [extensions], and returns
  // a list of resulting pseudo selectors.
  // ##########################################################################
  std::vector<Pseudo_Selector_Obj> Extender::extendPseudo(
    Pseudo_Selector_Obj pseudo,
    ExtSelExtMap& extensions,
    CssMediaRule_Obj mediaQueryContext)
  {
    auto selector = pseudo->selector();
    SelectorListObj extended = extendList(
      selector, extensions, mediaQueryContext);
    if (!extended || !pseudo || !pseudo->selector()) { return {}; }
    if (ObjEqualityFn(pseudo->selector(), extended)) { return {}; }

    // For `:not()`, we usually want to get rid of any complex selectors because
    // that will cause the selector to fail to parse on all browsers at time of
    // writing. We can keep them if either the original selector had a complex
    // selector, or the result of extending has only complex selectors, because
    // either way we aren't breaking anything that isn't already broken.
    std::vector<ComplexSelectorObj> complexes = extended->elements();

    if (pseudo->normalized() == "not") {
      if (!hasAny(pseudo->selector()->elements(), hasMoreThanOne)) {
        if (hasAny(extended->elements(), hasExactlyOne)) {
          complexes.clear();
          for (auto complex : extended->elements()) {
            if (complex->length() <= 1) {
              complexes.push_back(complex);
            }
          }
        }
      }
    }
    
    std::vector<ComplexSelectorObj> expanded = expand(
      complexes, extendPseudoComplex, pseudo, mediaQueryContext);

    // Older browsers support `:not`, but only with a single complex selector.
    // In order to support those browsers, we break up the contents of a `:not`
    // unless it originally contained a selector list.
    if (pseudo->normalized() == "not") {
      if (pseudo->selector()->length() == 1) {
        std::vector<Pseudo_Selector_Obj> pseudos;
        for (size_t i = 0; i < expanded.size(); i += 1) {
          pseudos.push_back(pseudo->withSelector(
            expanded[i]->wrapInList()
          ));
        }
        return pseudos;
      }
    }

    SelectorListObj list = SASS_MEMORY_NEW(SelectorList, "[phony]");
    return { pseudo->withSelector(list->concat(complexes)) };

  }
  // EO extendPseudo

  // ##########################################################################
  // ##########################################################################
  bool dontTrimComplex(
    const ComplexSelector* complex2,
    const ComplexSelector* complex1,
    const size_t maxSpecificity)
  {
    if (complex2->minSpecificity() < maxSpecificity) return false;
    return complex2->isSuperselectorOf(complex1);
  }

  // ##########################################################################
  // Rotates the element in list from [start] (inclusive) to [end] (exclusive)
  // one index higher, looping the final element back to [start].
  // ##########################################################################
  void rotateSlice(std::vector<ComplexSelectorObj>& list, size_t start, size_t end) {
    auto element = list[end - 1];
    for (size_t i = start; i < end; i++) {
      auto next = list[i];
      list[i] = element;
      element = next;
    }
  }

  // ##########################################################################
  // Removes elements from [selectors] if they're subselectors of other
  // elements. The [isOriginal] callback indicates which selectors are
  // original to the document, and thus should never be trimmed.
  // ##########################################################################
  // Note: for adaption I pass in the set directly, there is some
  // code path in selector-trim that might need this special callback
  // ##########################################################################
  std::vector<ComplexSelectorObj> Extender::trim(
    std::vector<ComplexSelectorObj> selectors,
    ExtCplxSelSet& existing)
  {

    // Avoid truly horrific quadratic behavior.
    // TODO(nweiz): I think there may be a way to get perfect trimming 
    // without going quadratic by building some sort of trie-like
    // data structure that can be used to look up superselectors.
    // TODO(mgreter): Check how this perfoms in C++ (up the limit)
    if (selectors.size() > 100) return selectors;

    // This is n² on the sequences, but only comparing between separate sequences
    // should limit the quadratic behavior. We iterate from last to first and reverse
    // the result so that, if two selectors are identical, we keep the first one.
    std::vector<ComplexSelectorObj> result; size_t numOriginals = 0;

    // Use label to quit outer loop
  redo:

    for (size_t i = selectors.size() - 1; i != std::string::npos; i--) {
      const ComplexSelectorObj& complex1 = selectors[i];
      // Check if selector in known in existing "originals"
      // For custom behavior dart-sass had `isOriginal(complex1)`
      if (existing.find(complex1) != existing.end()) {
        // Make sure we don't include duplicate originals, which could
        // happen if a style rule extends a component of its own selector.
        for (size_t j = 0; j < numOriginals; j++) {
          if (*result[j] == *complex1) {
            rotateSlice(result, 0, j + 1);
            goto redo;
          }
        }
        result.insert(result.begin(), complex1);
        numOriginals++;
        continue;
      }

      // The maximum specificity of the sources that caused [complex1]
      // to be generated. In order for [complex1] to be removed, there
      // must be another selector that's a superselector of it *and*
      // that has specificity greater or equal to this.
      size_t maxSpecificity = 0;
      for (SelectorComponentObj component : complex1->elements()) {
        if (CompoundSelectorObj compound = Cast<CompoundSelector>(component)) {
          maxSpecificity = std::max(maxSpecificity, maxSourceSpecificity(compound));
        }
      }
      

      // Look in [result] rather than [selectors] for selectors after [i]. This
      // ensures we aren't comparing against a selector that's already been trimmed,
      // and thus that if there are two identical selectors only one is trimmed.
      if (hasAny(result, dontTrimComplex, complex1, maxSpecificity)) {
        continue;
      }

      // Check if any element (up to [i]) from [selector] returns true
      // when passed to [dontTrimComplex]. The arguments [complex1] and
      // [maxSepcificity] will be passed to the invoked function.
      if (hasSubAny(selectors, i, dontTrimComplex, complex1, maxSpecificity)) {
        continue;
      }

      // ToDo: Maybe use deque for front insert?
      result.insert(result.begin(), complex1);

    }

    return result;

  }
  // EO trim

  // Returns the maximum specificity of the given [simple] source selector.
  size_t Extender::maxSourceSpecificity(SimpleSelectorObj simple)
  {
    auto it = sourceSpecificity.find(simple);
    if (it == sourceSpecificity.end()) return 0;
    return it->second;
  }

  // Returns the maximum specificity for sources that went into producing [compound].
  size_t Extender::maxSourceSpecificity(CompoundSelectorObj compound)
  {
    size_t specificity = 0;
    for (auto simple : compound->elements()) {
      size_t src = maxSourceSpecificity(simple);
      specificity = std::max(specificity, src);
    }
    return specificity;
  }

}
