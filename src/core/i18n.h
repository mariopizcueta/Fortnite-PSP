// Idioma del juego (parche, fase 8): todos los textos visibles en un solo sitio
// (core/i18n_texts.h), en espanol e ingles. El codigo usa identificadores (i18n::T) y los
// traduce al dibujar con tr(), asi cambiar de idioma en los ajustes cambia toda la
// interfaz al momento y la logica del juego no se duplica.
#pragma once

#include <psptypes.h>

#include "core/i18n_texts.h"

namespace i18n {

enum class Lang : u8 { Es, En, Count };

enum class T : u16 {
#define FNPSP_TEXT_ID(id, es, en) id,
    FNPSP_TEXTS(FNPSP_TEXT_ID)
#undef FNPSP_TEXT_ID
    Count
};

void setLanguage(Lang lang);
Lang language();
const char* tr(T id);
// Nombre del idioma en su propio idioma (para los ajustes).
const char* languageName(Lang lang);

}  // namespace i18n
