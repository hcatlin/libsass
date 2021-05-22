/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_SOURCE_MAP_HPP
#define SASS_SOURCE_MAP_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "backtrace.hpp"
#include "base64vlq.hpp"
#include "mapping.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  // Helper class to create the source-mappings.
  /////////////////////////////////////////////////////////////////////////

  class SourceMap {

  private:

    // Sources by index
    sass::vector<size_t> sources;

    // List of all source mappings
    // Deque is not faster, I checked
    sass::vector<Mapping> mappings;

    // Current position
    Offset position;

  public:

    // Empty constructor
    SourceMap();

    // Reserve space beforehand
    void reserve(size_t size) {
      sources.reserve(size);
      mappings.reserve(size);
    }

    // Register a new source index
    void addSourceIndex(size_t idx) {
      sources.push_back(idx);
    }

    // Call when text in the original was appended
    void append(const Offset& offset);

    // Call when text in the original was prepended
    void prepend(const Offset& offset);

    // Call when another buffer is appended to the original
    void append(const OutputBuffer& out);

    // Call when another buffer is prepended to the original
    void prepend(const OutputBuffer& out);

    // Add mapping pointing to ast node start position
    void addOpenMapping(const AstNode* node);

    // Add mapping pointing to ast node end position
    void addCloseMapping(const AstNode* node);

    // Render the source-map into comma-separated base64 encoded representation
    sass::string render(const std::unordered_map<size_t, size_t>& remap_srcidx) const;

  };

  /////////////////////////////////////////////////////////////////////////
  // Helper class to hold output with srcmap attached.
  /////////////////////////////////////////////////////////////////////////

  class OutputBuffer {

  private:

    // Make sure we don't allow any copies
    OutputBuffer(const OutputBuffer&) = delete;
    OutputBuffer& operator=(const OutputBuffer&) = delete;

  public:

    // Main buffer string
    sass::string buffer;

    // Optional source map
    SourceMap* srcmap;

    // Default constructor
    OutputBuffer(bool srcmap = false) noexcept;

    // Allow to move the buffer
    OutputBuffer(OutputBuffer&&) noexcept;

  };

}

#endif
