// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/translator/implementations/yandex.h"

#include "ayu/features/translator/html_parser.h"

#include <memory>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ayu::Translator {

YandexTranslator &YandexTranslator::instance() {
	static YandexTranslator inst;
	return inst;
}

YandexTranslator::YandexTranslator(QObject *parent)
	: MultiThreadTranslator(parent) {
}

QSet<QString> YandexTranslator::supportedLanguages() const {
	static const QSet<QString> languages = {
		QStringLiteral("az"), QStringLiteral("sq"), QStringLiteral("am"), QStringLiteral("en"),
		QStringLiteral("ar"), QStringLiteral("hy"), QStringLiteral("af"), QStringLiteral("eu"),
		QStringLiteral("ba"), QStringLiteral("be"), QStringLiteral("bn"), QStringLiteral("my"),
		QStringLiteral("bg"), QStringLiteral("bs"), QStringLiteral("cv"), QStringLiteral("cy"),
		QStringLiteral("hu"), QStringLiteral("vi"), QStringLiteral("ht"), QStringLiteral("gl"),
		QStringLiteral("nl"), QStringLiteral("mrj"), QStringLiteral("el"), QStringLiteral("ka"),
		QStringLiteral("gu"), QStringLiteral("da"), QStringLiteral("he"), QStringLiteral("yi"),
		QStringLiteral("id"), QStringLiteral("ga"), QStringLiteral("it"), QStringLiteral("is"),
		QStringLiteral("es"), QStringLiteral("kk"), QStringLiteral("kn"), QStringLiteral("ca"),
		QStringLiteral("ky"), QStringLiteral("zh"), QStringLiteral("ko"), QStringLiteral("xh"),
		QStringLiteral("km"), QStringLiteral("lo"), QStringLiteral("la"), QStringLiteral("lv"),
		QStringLiteral("lt"), QStringLiteral("lb"), QStringLiteral("mg"), QStringLiteral("ms"),
		QStringLiteral("ml"), QStringLiteral("mt"), QStringLiteral("mk"), QStringLiteral("mi"),
		QStringLiteral("mr"), QStringLiteral("mhr"), QStringLiteral("mn"), QStringLiteral("de"),
		QStringLiteral("ne"), QStringLiteral("no"), QStringLiteral("pa"), QStringLiteral("pap"),
		QStringLiteral("fa"), QStringLiteral("pl"), QStringLiteral("pt"), QStringLiteral("ro"),
		QStringLiteral("ru"), QStringLiteral("ceb"), QStringLiteral("sr"), QStringLiteral("si"),
		QStringLiteral("sk"), QStringLiteral("sl"), QStringLiteral("sw"), QStringLiteral("su"),
		QStringLiteral("tg"), QStringLiteral("th"), QStringLiteral("tl"), QStringLiteral("ta"),
		QStringLiteral("tt"), QStringLiteral("te"), QStringLiteral("tr"), QStringLiteral("udm"),
		QStringLiteral("uz"), QStringLiteral("uk"), QStringLiteral("ur"), QStringLiteral("fi"),
		QStringLiteral("fr"), QStringLiteral("hi"), QStringLiteral("hr"), QStringLiteral("cs"),
		QStringLiteral("sv"), QStringLiteral("gd"), QStringLiteral("et"), QStringLiteral("eo"),
		QStringLiteral("jv"), QStringLiteral("ja")
	};
	return languages;
}

QPointer<QNetworkReply> YandexTranslator::startSingleTranslation(
	const MultiThreadArgs &args
) {
	// Disabled: no text is sent to Yandex. Returns "Disabled" as the translation.
	if (args.onSuccess) {
		TextWithEntities result;
		result.text = u"Disabled"_q;
		args.onSuccess(result);
	}
	return nullptr;
}

} // namespace Ayu::Translator
