// Tencent is pleased to support the open source community by making RapidJSON available.
//
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
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

#ifndef RAPIDJSON_ERROR_EN_H_
#define RAPIDJSON_ERROR_EN_H_

#include "error.h"

#ifdef __clang__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(switch-enum)
RAPIDJSON_DIAG_OFF(covered-switch-default)
#endif

RAPIDJSON_NAMESPACE_BEGIN

//! Maps error code of parsing into error message.
/*!
    \ingroup RAPIDJSON_ERRORS
    \param parseErrorCode Error code obtained in parsing.
    \return the error message.
    \note User can make a copy of this function for localization.
        Using switch-case is safer for future modification of error codes.
*/
inline const RAPIDJSON_ERROR_CHARTYPE* GetParseError_En(ParseErrorCode parseErrorCode) {
    switch (parseErrorCode) {
        case kParseErrorNone:                           return RAPIDJSON_ERROR_STRING("No error.");

        case kParseErrorDocumentEmpty:                  return RAPIDJSON_ERROR_STRING("The document is empty.");
        case kParseErrorDocumentRootNotSingular:        return RAPIDJSON_ERROR_STRING("The document root must not be followed by other values.");

        case kParseErrorValueInvalid:                   return RAPIDJSON_ERROR_STRING("Invalid value.");

        case kParseErrorObjectMissName:                 return RAPIDJSON_ERROR_STRING("Missing a name for object member.");
        case kParseErrorObjectMissColon:                return RAPIDJSON_ERROR_STRING("Missing a colon after a name of object member.");
        case kParseErrorObjectMissCommaOrCurlyBracket:  return RAPIDJSON_ERROR_STRING("Missing a comma or '}' after an object member.");

        case kParseErrorArrayMissCommaOrSquareBracket:  return RAPIDJSON_ERROR_STRING("Missing a comma or ']' after an array element.");

        case kParseErrorStringUnicodeEscapeInvalidHex:  return RAPIDJSON_ERROR_STRING("Incorrect hex digit after \\u escape in string.");
        case kParseErrorStringUnicodeSurrogateInvalid:  return RAPIDJSON_ERROR_STRING("The surrogate pair in string is invalid.");
        case kParseErrorStringEscapeInvalid:            return RAPIDJSON_ERROR_STRING("Invalid escape character in string.");
        case kParseErrorStringMissQuotationMark:        return RAPIDJSON_ERROR_STRING("Missing a closing quotation mark in string.");
        case kParseErrorStringInvalidEncoding:          return RAPIDJSON_ERROR_STRING("Invalid encoding in string.");

        case kParseErrorNumberTooBig:                   return RAPIDJSON_ERROR_STRING("Number too big to be stored in double.");
        case kParseErrorNumberMissFraction:             return RAPIDJSON_ERROR_STRING("Miss fraction part in number.");
        case kParseErrorNumberMissExponent:             return RAPIDJSON_ERROR_STRING("Miss exponent in number.");

        case kParseErrorTermination:                    return RAPIDJSON_ERROR_STRING("Terminate parsing due to Handler error.");
        case kParseErrorUnspecificSyntaxError:          return RAPIDJSON_ERROR_STRING("Unspecific syntax error.");

        default:                                        return RAPIDJSON_ERROR_STRING("Unknown error.");
    }
}

//! Maps error code of validation into error message.
/*!
    \ingroup RAPIDJSON_ERRORS
    \param validateErrorCode Error code obtained from validator.
    \return the error message.
    \note User can make a copy of this function for localization.
        Using switch-case is safer for future modification of error codes.
*/
inline const RAPIDJSON_ERROR_CHARTYPE* GetValidateError_En(ValidateErrorCode validateErrorCode) {
    switch (validateErrorCode) {
        case kValidateErrors:                           return RAPIDJSON_ERROR_STRING("One or more validation errors have occurred");
        case kValidateErrorNone:                        return RAPIDJSON_ERROR_STRING("No error.");

        case kValidateErrorMultipleOf:                  return RAPIDJSON_ERROR_STRING("Number '%actual' is not a multiple of the 'multipleOf' value '%expected'.");
        case kValidateErrorMaximum:                     return RAPIDJSON_ERROR_STRING("Number '%actual' is greater than the 'maximum' value '%expected'.");
        case kValidateErrorExclusiveMaximum:            return RAPIDJSON_ERROR_STRING("Number '%actual' is greater than or equal to the 'exclusiveMaximum' value '%expected'.");
        case kValidateErrorMinimum:                     return RAPIDJSON_ERROR_STRING("Number '%actual' is less than the 'minimum' value '%expected'.");
        case kValidateErrorExclusiveMinimum:            return RAPIDJSON_ERROR_STRING("Number '%actual' is less than or equal to the 'exclusiveMinimum' value '%expected'.");

        case kValidateErrorMaxLength:                   return RAPIDJSON_ERROR_STRING("String '%actual' is longer than the 'maxLength' value '%expected'.");
        case kValidateErrorMinLength:                   return RAPIDJSON_ERROR_STRING("String '%actual' is shorter than the 'minLength' value '%expected'.");
        case kValidateErrorPattern:                     return RAPIDJSON_ERROR_STRING("String '%actual' does not match the 'pattern' regular expression.");

        case kValidateErrorMaxItems:                    return RAPIDJSON_ERROR_STRING("Array of length '%actual' is longer than the 'maxItems' value '%expected'.");
        case kValidateErrorMinItems:                    return RAPIDJSON_ERROR_STRING("Array of length '%actual' is shorter than the 'minItems' value '%expected'.");
        case kValidateErrorUniqueItems:                 return RAPIDJSON_ERROR_STRING("Array has duplicate items at indices '%duplicates' but 'uniqueItems' is true.");
        case kValidateErrorAdditionalItems:             return RAPIDJSON_ERROR_STRING("Array has an additional item at index '%disallowed' that is not allowed by the schema.");

        case kValidateErrorMaxProperties:               return RAPIDJSON_ERROR_STRING("Object has '%actual' members which is more than 'maxProperties' value '%expected'.");
        case kValidateErrorMinProperties:               return RAPIDJSON_ERROR_STRING("Object has '%actual' members which is less than 'minProperties' value '%expected'.");
        case kValidateErrorRequired:                    return RAPIDJSON_ERROR_STRING("Object is missing the following members required by the schema: '%missing'.");
        case kValidateErrorAdditionalProperties:        return RAPIDJSON_ERROR_STRING("Object has an additional member '%disallowed' that is not allowed by the schema.");
        case kValidateErrorPatternProperties:           return RAPIDJSON_ERROR_STRING("Object has 'patternProperties' that are not allowed by the schema.");
        case kValidateErrorDependencies:                return RAPIDJSON_ERROR_STRING("Object has missing property or schema dependencies, refer to following errors.");

        case kValidateErrorEnum:                        return RAPIDJSON_ERROR_STRING("Property has a value that is not one of its allowed enumerated values.");
        case kValidateErrorType:                        return RAPIDJSON_ERROR_STRING("Property has a type '%actual' that is not in the following list: '%expected'.");

        case kValidateErrorOneOf:                       return RAPIDJSON_ERROR_STRING("Property did not match any of the sub-schemas specified by 'oneOf', refer to following errors.");
        case kValidateErrorOneOfMatch:                  return RAPIDJSON_ERROR_STRING("Property matched more than one of the sub-schemas specified by 'oneOf', indices '%matches'.");
        case kValidateErrorAllOf:                       return RAPIDJSON_ERROR_STRING("Property did not match all of the sub-schemas specified by 'allOf', refer to following errors.");
        case kValidateErrorAnyOf:                       return RAPIDJSON_ERROR_STRING("Property did not match any of the sub-schemas specified by 'anyOf', refer to following errors.");
        case kValidateErrorNot:                         return RAPIDJSON_ERROR_STRING("Property matched the sub-schema specified by 'not'.");

        case kValidateErrorReadOnly:                    return RAPIDJSON_ERROR_STRING("Property is read-only but has been provided when validation is for writing.");
        case kValidateErrorWriteOnly:                   return RAPIDJSON_ERROR_STRING("Property is write-only but has been provided when validation is for reading.");

        default:                                        return RAPIDJSON_ERROR_STRING("Unknown error.");
    }
}

//! Maps error code of schema document compilation into error message.
/*!
    \ingroup RAPIDJSON_ERRORS
    \param schemaErrorCode Error code obtained from compiling the schema document.
    \return the error message.
    \note User can make a copy of this function for localization.
        Using switch-case is safer for future modification of error codes.
*/
  inline const RAPIDJSON_ERROR_CHARTYPE* GetSchemaError_En(SchemaErrorCode schemaErrorCode) {
      switch (schemaErrorCode) {
          case kSchemaErrorNone:                        return RAPIDJSON_ERROR_STRING("No error.");

          case kSchemaErrorStartUnknown:                return RAPIDJSON_ERROR_STRING("Pointer '%value' to start of schema does not resolve to a location in the document.");
          case kSchemaErrorRefPlainName:                return RAPIDJSON_ERROR_STRING("$ref fragment '%value' must be a JSON pointer.");
          case kSchemaErrorRefInvalid:                  return RAPIDJSON_ERROR_STRING("$ref must not be an empty string.");
          case kSchemaErrorRefPointerInvalid:           return RAPIDJSON_ERROR_STRING("$ref fragment '%value' is not a valid JSON pointer at offset '%offset'.");
          case kSchemaErrorRefUnknown:                  return RAPIDJSON_ERROR_STRING("$ref '%value' does not resolve to a location in the target document.");
          case kSchemaErrorRefCyclical:                 return RAPIDJSON_ERROR_STRING("$ref '%value' is cyclical.");
          case kSchemaErrorRefNoRemoteProvider:         return RAPIDJSON_ERROR_STRING("$ref is remote but there is no remote provider.");
          case kSchemaErrorRefNoRemoteSchema:           return RAPIDJSON_ERROR_STRING("$ref '%value' is remote but the remote provider did not return a schema.");
          case kSchemaErrorRegexInvalid:                return RAPIDJSON_ERROR_STRING("Invalid regular expression '%value' in 'pattern' or 'patternProperties'.");
          case kSchemaErrorSpecUnknown:                 return RAPIDJSON_ERROR_STRING("JSON schema draft or OpenAPI version is not recognized.");
          case kSchemaErrorSpecUnsupported:             return RAPIDJSON_ERROR_STRING("JSON schema draft or OpenAPI version is not supported.");
          case kSchemaErrorSpecIllegal:                 return RAPIDJSON_ERROR_STRING("Both JSON schema draft and OpenAPI version found in document.");
          case kSchemaErrorReadOnlyAndWriteOnly:        return RAPIDJSON_ERROR_STRING("Property must not be both 'readOnly' and 'writeOnly'.");

          default:                                      return RAPIDJSON_ERROR_STRING("Unknown error.");
    }
  }

//! Maps error code of pointer parse into error message.
/*!
    \ingroup RAPIDJSON_ERRORS
    \param pointerParseErrorCode Error code obtained from pointer parse.
    \return the error message.
    \note User can make a copy of this function for localization.
        Using switch-case is safer for future modification of error codes.
*/
inline const RAPIDJSON_ERROR_CHARTYPE* GetPointerParseError_En(PointerParseErrorCode pointerParseErrorCode) {
    switch (pointerParseErrorCode) {
        case kPointerParseErrorNone:                       return RAPIDJSON_ERROR_STRING("No error.");

        case kPointerParseErrorTokenMustBeginWithSolidus:  return RAPIDJSON_ERROR_STRING("A token must begin with a '/'.");
        case kPointerParseErrorInvalidEscape:              return RAPIDJSON_ERROR_STRING("Invalid escape.");
        case kPointerParseErrorInvalidPercentEncoding:     return RAPIDJSON_ERROR_STRING("Invalid percent encoding in URI fragment.");
        case kPointerParseErrorCharacterMustPercentEncode: return RAPIDJSON_ERROR_STRING("A character must be percent encoded in a URI fragment.");

        default:                                           return RAPIDJSON_ERROR_STRING("Unknown error.");
    }
}

RAPIDJSON_NAMESPACE_END

#ifdef __clang__
RAPIDJSON_DIAG_POP
#endif

#endif // RAPIDJSON_ERROR_EN_H_
