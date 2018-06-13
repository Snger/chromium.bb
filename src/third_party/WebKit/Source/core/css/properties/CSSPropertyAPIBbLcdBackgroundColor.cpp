#include "core/css/properties/CSSPropertyAPIBbLcdBackgroundColor.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPILineHeight::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {

  if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeColor(range, cssParserMode);
}

}  // namespace blink