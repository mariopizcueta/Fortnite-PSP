#include "core/i18n.h"

namespace i18n {
namespace {

const char* const kSpanish[] = {
#define FNPSP_TEXT_ES(id, es, en) es,
    FNPSP_TEXTS(FNPSP_TEXT_ES)
#undef FNPSP_TEXT_ES
};

const char* const kEnglish[] = {
#define FNPSP_TEXT_EN(id, es, en) en,
    FNPSP_TEXTS(FNPSP_TEXT_EN)
#undef FNPSP_TEXT_EN
};

static_assert(sizeof(kSpanish) / sizeof(kSpanish[0]) == static_cast<int>(T::Count), "tabla de espanol incompleta");
static_assert(sizeof(kEnglish) / sizeof(kEnglish[0]) == static_cast<int>(T::Count), "tabla de ingles incompleta");

Lang g_language = Lang::Es;

}  // namespace

void setLanguage(Lang lang) { g_language = lang == Lang::En ? Lang::En : Lang::Es; }

Lang language() { return g_language; }

const char* tr(T id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(T::Count)) return "";
    return g_language == Lang::En ? kEnglish[i] : kSpanish[i];
}

const char* languageName(Lang lang) { return lang == Lang::En ? "English" : "Espanol"; }

}  // namespace i18n
