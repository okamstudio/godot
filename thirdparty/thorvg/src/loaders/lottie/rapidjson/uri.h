// Tencent is pleased to support the open source community by making RapidJSON available.
//
// (C) Copyright IBM Corporation 2021
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef RAPIDJSON_URI_H_
#define RAPIDJSON_URI_H_

#include "internal/strfunc.h"

#if defined(__clang__)
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(c++98-compat)
#elif defined(_MSC_VER)
RAPIDJSON_DIAG_OFF(4512) // assignment operator could not be generated
#endif

RAPIDJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// GenericUri

template <typename ValueType, typename Allocator=CrtAllocator>
class GenericUri {
public:
    typedef typename ValueType::Ch Ch;
#if RAPIDJSON_HAS_STDSTRING
    typedef std::basic_string<Ch> String;
#endif

    //! Constructors
    GenericUri(Allocator* allocator = 0) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
    }

    GenericUri(const Ch* uri, SizeType len, Allocator* allocator = 0) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
        Parse(uri, len);
    }

    GenericUri(const Ch* uri, Allocator* allocator = 0) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
        Parse(uri, internal::StrLen<Ch>(uri));
    }

    // Use with specializations of GenericValue
    template<typename T> GenericUri(const T& uri, Allocator* allocator = 0) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
        const Ch* u = uri.template Get<const Ch*>(); // TypeHelper from document.h
        Parse(u, internal::StrLen<Ch>(u));
    }

#if RAPIDJSON_HAS_STDSTRING
    GenericUri(const String& uri, Allocator* allocator = 0) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
        Parse(uri.c_str(), internal::StrLen<Ch>(uri.c_str()));
    }
#endif

    //! Copy constructor
    GenericUri(const GenericUri& rhs) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(), ownAllocator_() {
        *this = rhs;
    }

    //! Copy constructor
    GenericUri(const GenericUri& rhs, Allocator* allocator) : uri_(), base_(), scheme_(), auth_(), path_(), query_(), frag_(), allocator_(allocator), ownAllocator_() {
        *this = rhs;
    }

    //! Destructor.
    ~GenericUri() {
        Free();
        RAPIDJSON_DELETE(ownAllocator_);
    }

    //! Assignment operator
    GenericUri& operator=(const GenericUri& rhs) {
        if (this != &rhs) {
            // Do not delete ownAllocator
            Free();
            Allocate(rhs.GetStringLength());
            auth_ = CopyPart(scheme_, rhs.scheme_, rhs.GetSchemeStringLength());
            path_ = CopyPart(auth_, rhs.auth_, rhs.GetAuthStringLength());
            query_ = CopyPart(path_, rhs.path_, rhs.GetPathStringLength());
            frag_ = CopyPart(query_, rhs.query_, rhs.GetQueryStringLength());
            base_ = CopyPart(frag_, rhs.frag_, rhs.GetFragStringLength());
            uri_ = CopyPart(base_, rhs.base_, rhs.GetBaseStringLength());
            CopyPart(uri_, rhs.uri_, rhs.GetStringLength());
        }
        return *this;
    }

    //! Getters
    // Use with specializations of GenericValue
    template<typename T> void Get(T& uri, Allocator& allocator) {
        uri.template Set<const Ch*>(this->GetString(), allocator); // TypeHelper from document.h
    }

    const Ch* GetString() const { return uri_; }
    SizeType GetStringLength() const { return uri_ == 0 ? 0 : internal::StrLen<Ch>(uri_); }
    const Ch* GetBaseString() const { return base_; }
    SizeType GetBaseStringLength() const { return base_ == 0 ? 0 : internal::StrLen<Ch>(base_); }
    const Ch* GetSchemeString() const { return scheme_; }
    SizeType GetSchemeStringLength() const { return scheme_ == 0 ? 0 : internal::StrLen<Ch>(scheme_); }
    const Ch* GetAuthString() const { return auth_; }
    SizeType GetAuthStringLength() const { return auth_ == 0 ? 0 : internal::StrLen<Ch>(auth_); }
    const Ch* GetPathString() const { return path_; }
    SizeType GetPathStringLength() const { return path_ == 0 ? 0 : internal::StrLen<Ch>(path_); }
    const Ch* GetQueryString() const { return query_; }
    SizeType GetQueryStringLength() const { return query_ == 0 ? 0 : internal::StrLen<Ch>(query_); }
    const Ch* GetFragString() const { return frag_; }
    SizeType GetFragStringLength() const { return frag_ == 0 ? 0 : internal::StrLen<Ch>(frag_); }

#if RAPIDJSON_HAS_STDSTRING
    static String Get(const GenericUri& uri) { return String(uri.GetString(), uri.GetStringLength()); }
    static String GetBase(const GenericUri& uri) { return String(uri.GetBaseString(), uri.GetBaseStringLength()); }
    static String GetScheme(const GenericUri& uri) { return String(uri.GetSchemeString(), uri.GetSchemeStringLength()); }
    static String GetAuth(const GenericUri& uri) { return String(uri.GetAuthString(), uri.GetAuthStringLength()); }
    static String GetPath(const GenericUri& uri) { return String(uri.GetPathString(), uri.GetPathStringLength()); }
    static String GetQuery(const GenericUri& uri) { return String(uri.GetQueryString(), uri.GetQueryStringLength()); }
    static String GetFrag(const GenericUri& uri) { return String(uri.GetFragString(), uri.GetFragStringLength()); }
#endif

    //! Equality operators
    bool operator==(const GenericUri& rhs) const {
        return Match(rhs, true);
    }

    bool operator!=(const GenericUri& rhs) const {
        return !Match(rhs, true);
    }

    bool Match(const GenericUri& uri, bool full = true) const {
        Ch* s1;
        Ch* s2;
        if (full) {
            s1 = uri_;
            s2 = uri.uri_;
        } else {
            s1 = base_;
            s2 = uri.base_;
        }
        if (s1 == s2) return true;
        if (s1 == 0 || s2 == 0) return false;
        return internal::StrCmp<Ch>(s1, s2) == 0;
    }

    //! Resolve this URI against another (base) URI in accordance with URI resolution rules.
    // See https://tools.ietf.org/html/rfc3986
    // Use for resolving an id or $ref with an in-scope id.
    // Returns a new GenericUri for the resolved URI.
    GenericUri Resolve(const GenericUri& baseuri, Allocator* allocator = 0) {
        GenericUri resuri;
        resuri.allocator_ = allocator;
        // Ensure enough space for combining paths
        resuri.Allocate(GetStringLength() + baseuri.GetStringLength() + 1); // + 1 for joining slash

        if (!(GetSchemeStringLength() == 0)) {
            // Use all of this URI
            resuri.auth_ = CopyPart(resuri.scheme_, scheme_, GetSchemeStringLength());
            resuri.path_ = CopyPart(resuri.auth_, auth_, GetAuthStringLength());
            resuri.query_ = CopyPart(resuri.path_, path_, GetPathStringLength());
            resuri.frag_ = CopyPart(resuri.query_, query_, GetQueryStringLength());
            resuri.RemoveDotSegments();
        } else {
            // Use the base scheme
            resuri.auth_ = CopyPart(resuri.scheme_, baseuri.scheme_, baseuri.GetSchemeStringLength());
            if (!(GetAuthStringLength() == 0)) {
                // Use this auth, path, query
                resuri.path_ = CopyPart(resuri.auth_, auth_, GetAuthStringLength());
                resuri.query_ = CopyPart(resuri.path_, path_, GetPathStringLength());
                resuri.frag_ = CopyPart(resuri.query_, query_, GetQueryStringLength());
                resuri.RemoveDotSegments();
            } else {
                // Use the base auth
                resuri.path_ = CopyPart(resuri.auth_, baseuri.auth_, baseuri.GetAuthStringLength());
                if (GetPathStringLength() == 0) {
                    // Use the base path
                    resuri.query_ = CopyPart(resuri.path_, baseuri.path_, baseuri.GetPathStringLength());
                    if (GetQueryStringLength() == 0) {
                        // Use the base query
                        resuri.frag_ = CopyPart(resuri.query_, baseuri.query_, baseuri.GetQueryStringLength());
                    } else {
                        // Use this query
                        resuri.frag_ = CopyPart(resuri.query_, query_, GetQueryStringLength());
                    }
                } else {
                    if (path_[0] == '/') {
                        // Absolute path - use all of this path
                        resuri.query_ = CopyPart(resuri.path_, path_, GetPathStringLength());
                        resuri.RemoveDotSegments();
                    } else {
                        // Relative path - append this path to base path after base path's last slash
                        size_t pos = 0;
                        if (!(baseuri.GetAuthStringLength() == 0) && baseuri.GetPathStringLength() == 0) {
                            resuri.path_[pos] = '/';
                            pos++;
                        }
                        size_t lastslashpos = baseuri.GetPathStringLength();
                        while (lastslashpos > 0) {
                            if (baseuri.path_[lastslashpos - 1] == '/') break;
                            lastslashpos--;
                        }
                        std::memcpy(&resuri.path_[pos], baseuri.path_, lastslashpos * sizeof(Ch));
                        pos += lastslashpos;
                        resuri.query_ = CopyPart(&resuri.path_[pos], path_, GetPathStringLength());
                        resuri.RemoveDotSegments();
                    }
                    // Use this query
                    resuri.frag_ = CopyPart(resuri.query_, query_, GetQueryStringLength());
                }
            }
        }
        // Always use this frag
        resuri.base_ = CopyPart(resuri.frag_, frag_, GetFragStringLength());

        // Re-constitute base_ and uri_
        resuri.SetBase();
        resuri.uri_ = resuri.base_ + resuri.GetBaseStringLength() + 1;
        resuri.SetUri();
        return resuri;
    }

    //! Get the allocator of this GenericUri.
    Allocator& GetAllocator() { return *allocator_; }

private:
    // Allocate memory for a URI
    // Returns total amount allocated
    std::size_t Allocate(std::size_t len) {
        // Create own allocator if user did not supply.
        if (!allocator_)
            ownAllocator_ =  allocator_ = RAPIDJSON_NEW(Allocator)();

        // Allocate one block containing each part of the URI (5) plus base plus full URI, all null terminated.
        // Order: scheme, auth, path, query, frag, base, uri
        // Note need to set, increment, assign in 3 stages to avoid compiler warning bug.
        size_t total = (3 * len + 7) * sizeof(Ch);
        scheme_ = static_cast<Ch*>(allocator_->Malloc(total));
        *scheme_ = '\0';
        auth_ = scheme_;
        auth_++;
        *auth_ = '\0';
        path_ = auth_;
        path_++;
        *path_ = '\0';
        query_ = path_;
        query_++;
        *query_ = '\0';
        frag_ = query_;
        frag_++;
        *frag_ = '\0';
        base_ = frag_;
        base_++;
        *base_ = '\0';
        uri_ = base_;
        uri_++;
        *uri_ = '\0';
        return total;
    }

    // Free memory for a URI
    void Free() {
        if (scheme_) {
            Allocator::Free(scheme_);
            scheme_ = 0;
        }
    }

    // Parse a URI into constituent scheme, authority, path, query, & fragment parts
    // Supports URIs that match regex ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))? as per
    // https://tools.ietf.org/html/rfc3986
    void Parse(const Ch* uri, std::size_t len) {
        std::size_t start = 0, pos1 = 0, pos2 = 0;
        Allocate(len);

        // Look for scheme ([^:/?#]+):)?
        if (start < len) {
            while (pos1 < len) {
                if (uri[pos1] == ':') break;
                pos1++;
            }
            if (pos1 != len) {
                while (pos2 < len) {
                    if (uri[pos2] == '/') break;
                    if (uri[pos2] == '?') break;
                    if (uri[pos2] == '#') break;
                    pos2++;
                }
                if (pos1 < pos2) {
                    pos1++;
                    std::memcpy(scheme_, &uri[start], pos1 * sizeof(Ch));
                    scheme_[pos1] = '\0';
                    start = pos1;
                }
            }
        }
        // Look for auth (//([^/?#]*))?
        // Note need to set, increment, assign in 3 stages to avoid compiler warning bug.
        auth_ = scheme_ + GetSchemeStringLength();
        auth_++;
        *auth_ = '\0';
        if (start < len - 1 && uri[start] == '/' && uri[start + 1] == '/') {
            pos2 = start + 2;
            while (pos2 < len) {
                if (uri[pos2] == '/') break;
                if (uri[pos2] == '?') break;
                if (uri[pos2] == '#') break;
                pos2++;
            }
            std::memcpy(auth_, &uri[start], (pos2 - start) * sizeof(Ch));
            auth_[pos2 - start] = '\0';
            start = pos2;
        }
        // Look for path ([^?#]*)
        // Note need to set, increment, assign in 3 stages to avoid compiler warning bug.
        path_ = auth_ + GetAuthStringLength();
        path_++;
        *path_ = '\0';
        if (start < len) {
            pos2 = start;
            while (pos2 < len) {
                if (uri[pos2] == '?') break;
                if (uri[pos2] == '#') break;
                pos2++;
            }
            if (start != pos2) {
                std::memcpy(path_, &uri[start], (pos2 - start) * sizeof(Ch));
                path_[pos2 - start] = '\0';
                if (path_[0] == '/')
                    RemoveDotSegments();   // absolute path - normalize
                start = pos2;
            }
        }
        // Look for query (\?([^#]*))?
        // Note need to set, increment, assign in 3 stages to avoid compiler warning bug.
        query_ = path_ + GetPathStringLength();
        query_++;
        *query_ = '\0';
        if (start < len && uri[start] == '?') {
            pos2 = start + 1;
            while (pos2 < len) {
                if (uri[pos2] == '#') break;
                pos2++;
            }
            if (start != pos2) {
                std::memcpy(query_, &uri[start], (pos2 - start) * sizeof(Ch));
                query_[pos2 - start] = '\0';
                start = pos2;
            }
        }
        // Look for fragment (#(.*))?
        // Note need to set, increment, assign in 3 stages to avoid compiler warning bug.
        frag_ = query_ + GetQueryStringLength();
        frag_++;
        *frag_ = '\0';
        if (start < len && uri[start] == '#') {
            std::memcpy(frag_, &uri[start], (len - start) * sizeof(Ch));
            frag_[len - start] = '\0';
        }

        // Re-constitute base_ and uri_
        base_ = frag_ + GetFragStringLength() + 1;
        SetBase();
        uri_ = base_ + GetBaseStringLength() + 1;
        SetUri();
    }

    // Reconstitute base
    void SetBase() {
        Ch* next = base_;
        std::memcpy(next, scheme_, GetSchemeStringLength() * sizeof(Ch));
        next+= GetSchemeStringLength();
        std::memcpy(next, auth_, GetAuthStringLength() * sizeof(Ch));
        next+= GetAuthStringLength();
        std::memcpy(next, path_, GetPathStringLength() * sizeof(Ch));
        next+= GetPathStringLength();
        std::memcpy(next, query_, GetQueryStringLength() * sizeof(Ch));
        next+= GetQueryStringLength();
        *next = '\0';
    }

    // Reconstitute uri
    void SetUri() {
        Ch* next = uri_;
        std::memcpy(next, base_, GetBaseStringLength() * sizeof(Ch));
        next+= GetBaseStringLength();
        std::memcpy(next, frag_, GetFragStringLength() * sizeof(Ch));
        next+= GetFragStringLength();
        *next = '\0';
    }

    // Copy a part from one GenericUri to another
    // Return the pointer to the next part to be copied to
    Ch* CopyPart(Ch* to, Ch* from, std::size_t len) {
        RAPIDJSON_ASSERT(to != 0);
        RAPIDJSON_ASSERT(from != 0);
        std::memcpy(to, from, len * sizeof(Ch));
        to[len] = '\0';
        Ch* next = to + len + 1;
        return next;
    }

    // Remove . and .. segments from the path_ member.
    // https://tools.ietf.org/html/rfc3986
    // This is done in place as we are only removing segments.
    void RemoveDotSegments() {
        std::size_t pathlen = GetPathStringLength();
        std::size_t pathpos = 0;  // Position in path_
        std::size_t newpos = 0;   // Position in new path_

        // Loop through each segment in original path_
        while (pathpos < pathlen) {
            // Get next segment, bounded by '/' or end
            size_t slashpos = 0;
            while ((pathpos + slashpos) < pathlen) {
                if (path_[pathpos + slashpos] == '/') break;
                slashpos++;
            }
            // Check for .. and . segments
            if (slashpos == 2 && path_[pathpos] == '.' && path_[pathpos + 1] == '.') {
                // Backup a .. segment in the new path_
                // We expect to find a previously added slash at the end or nothing
                RAPIDJSON_ASSERT(newpos == 0 || path_[newpos - 1] == '/');
                size_t lastslashpos = newpos;
                // Make sure we don't go beyond the start segment
                if (lastslashpos > 1) {
                    // Find the next to last slash and back up to it
                    lastslashpos--;
                    while (lastslashpos > 0) {
                        if (path_[lastslashpos - 1] == '/') break;
                        lastslashpos--;
                    }
                    // Set the new path_ position
                    newpos = lastslashpos;
                }
            } else if (slashpos == 1 && path_[pathpos] == '.') {
                // Discard . segment, leaves new path_ unchanged
            } else {
                // Move any other kind of segment to the new path_
                RAPIDJSON_ASSERT(newpos <= pathpos);
                std::memmove(&path_[newpos], &path_[pathpos], slashpos * sizeof(Ch));
                newpos += slashpos;
                // Add slash if not at end
                if ((pathpos + slashpos) < pathlen) {
                    path_[newpos] = '/';
                    newpos++;
                }
            }
            // Move to next segment
            pathpos += slashpos + 1;
        }
        path_[newpos] = '\0';
    }

    Ch* uri_;    // Everything
    Ch* base_;   // Everything except fragment
    Ch* scheme_; // Includes the :
    Ch* auth_;   // Includes the //
    Ch* path_;   // Absolute if starts with /
    Ch* query_;  // Includes the ?
    Ch* frag_;   // Includes the #

    Allocator* allocator_;      //!< The current allocator. It is either user-supplied or equal to ownAllocator_.
    Allocator* ownAllocator_;   //!< Allocator owned by this Uri.
};

//! GenericUri for Value (UTF-8, default allocator).
typedef GenericUri<Value> Uri;

RAPIDJSON_NAMESPACE_END

#if defined(__clang__)
RAPIDJSON_DIAG_POP
#endif

#endif // RAPIDJSON_URI_H_
