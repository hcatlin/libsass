#ifndef SASS_LEXER_H
#define SASS_LEXER_H

#include <cstring>

namespace Sass {
  namespace Prelexer {

    //####################################
    // BASIC CHARACTER MATCHERS
    //####################################

    // These are locale independant
    const bool is_space(const char& src);
    const bool is_alpha(const char& src);
    const bool is_punct(const char& src);
    const bool is_digit(const char& src);
    const bool is_alnum(const char& src);
    const bool is_xdigit(const char& src);
    const bool is_unicode(const char& src);
    const bool is_character(const char& src);

    // Match a single ctype predicate.
    const char* space(const char* src);
    const char* alpha(const char* src);
    const char* digit(const char* src);
    const char* xdigit(const char* src);
    const char* alnum(const char* src);
    const char* punct(const char* src);
    const char* unicode(const char* src);
    const char* character(const char* src);

    // Match multiple ctype characters.
    const char* spaces(const char* src);
    const char* digits(const char* src);

    // Whitespace handling.
    const char* no_spaces(const char* src);
    const char* optional_spaces(const char* src);

    // Match any single character (/./).
    const char* any_char(const char* src);

    // Assert word boundary (/\b/)
    // Is a zero-width positive lookaheads
    const char* word_boundary(const char* src);

    // Match a single linebreak (/(?:\n|\r\n?)/).
    const char* re_linebreak(const char* src);

    // Assert string boundaries (/\Z|\z|\A/)
    // There are zero-width positive lookaheads
    const char* end_of_line(const char* src);
    // const char* end_of_string(const char* src);
    // const char* start_of_string(const char* src);

    // Type definition for prelexer functions
    typedef const char* (*prelexer)(const char*);

    //####################################
    // BASIC "REGEX" CONSTRUCTORS
    //####################################

    // Match a single character literal.
    // Regex equivalent: /(?:literal)/
    template <char pre>
    const char* exactly(const char* src) {
      return *src == pre ? src + 1 : 0;
    }

    // Match a string constant.
    // Regex equivalent: /[axy]/
    template <const char* prefix>
    const char* exactly(const char* src) {
      if (prefix == 0) return 0;
      const char* pre = prefix;
      if (src == 0) return 0;
      // there is a small chance that the search prefix
      // is longer than the rest of the string to look at
      while (*pre && *src == *pre) {
        ++src, ++pre;
      }
      return *pre ? 0 : src;
    }

    // Match for members of char class.
    // Regex equivalent: /[axy]/
    template <const char* char_class>
    const char* class_char(const char* src) {
      const char* cc = char_class;
      while (*cc && *src != *cc) ++cc;
      return *cc ? src + 1 : 0;
    }

    // Match for members of char class.
    // Regex equivalent: /[axy]/
    template <const char* char_class>
    const char* class_chars(const char* src) {
      const char* p = src;
      while (class_char<char_class>(p)) ++p;
      return p == src ? 0 : p;
    }

    // Match all except the supplied one.
    // Regex equivalent: /[^x]/
    template <const char c>
    const char* any_char_but(const char* src) {
      return (*src && *src != c) ? src + 1 : 0;
    }

    // Succeeds if the matcher fails.
    // Aka. zero-width negative lookahead.
    // Regex equivalent: /(?!literal)/
    template <prelexer mx>
    const char* negate(const char* src) {
      return mx(src) ? 0 : src;
    }

    // Succeeds if the matcher succeeds.
    // Aka. zero-width positive lookahead.
    // Regex equivalent: /(?=literal)/
    // just hangs around until we need it
    // template <prelexer mx>
    // const char* lookahead(const char* src) {
    //   return mx(src) ? src : 0;
    // }

    // Tries supplied matchers in order.
    // Succeeds if one of them succeeds.
    // Regex equivalent: /(?:FOO|BAR)/
    template <prelexer... mxs>
    const char* alternatives(const char* src) {
      const char* rslt;
      for (prelexer mx : { mxs... }) {
        if ((rslt = mx(src))) return rslt;
      }
      return 0;
    }

    // Tries supplied matchers in order.
    // Succeeds if all of them succeeds.
    // Regex equivalent: /(?:FOO)(?:BAR)/
    template <prelexer... mxs>
    const char* sequence(const char* src) {
      const char* rslt = src;
      for (prelexer mx : { mxs... }) {
        if (!(rslt = mx(rslt))) return 0;
      }
      return rslt;
    }

    // Match a pattern or not. Always succeeds.
    // Regex equivalent: /(?:literal)?/
    template <prelexer mx>
    const char* optional(const char* src) {
      const char* p = mx(src);
      return p ? p : src;
    }

    // Match zero or more of the patterns.
    // Regex equivalent: /(?:literal)*/
    template <prelexer mx>
    const char* zero_plus(const char* src) {
      const char* p = mx(src);
      while (p) src = p, p = mx(src);
      return src;
    }

    // Match one or more of the patterns.
    // Regex equivalent: /(?:literal)+/
    template <prelexer mx>
    const char* one_plus(const char* src) {
      const char* p = mx(src);
      if (!p) return 0;
      while (p) src = p, p = mx(src);
      return src;
    }

    // Match mx non-greedy until delimiter.
    // Other prelexers are greedy by default.
    // Regex equivalent: /(?:$mx)*?(?=$delim)\b/
    template <prelexer mx, prelexer delim>
    const char* non_greedy(const char* src) {
      while (!delim(src)) {
        const char* p = mx(src);
        if (p == src) return 0;
        if (p == 0) return 0;
        src = p;
      }
      return src;
    }

    //####################################
    // ADVANCED "REGEX" CONSTRUCTORS
    //####################################

    // Match with word boundary rule.
    // Regex equivalent: /(?:$mx)\b/
    template <const char* mx>
    const char* word(const char* src) {
      return sequence <
               exactly < mx >,
               word_boundary
             >(src);
    }

  }
}

#endif
