#include "core/css/properties/CSSPropertyAPIBbLcdBackgroundColor.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIBbLcdBackgroundColor::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {

  if (range.Peek().Id() == CSSValueAuto || range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

}  // namespace blink