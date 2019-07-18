#include "uritests.h"
#include "../guiutil.h"
#include "../walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?req-dontexist="));
    QVERIFY(!GUIUtil::parseMarscoinURI(uri, &rv));

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?dontexist="));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("MARSocNh46xszubKP94ikV3awGLxQfiscx"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("MARSocNh46xszubKP94ikV3awGLxQfiscx"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?amount=0.001"));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("MARSocNh46xszubKP94ikV3awGLxQfiscx"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("marscoin:LQDPC5rbjDB72fGFVHu4enYhxGAZuRiFh9?amount=1.001"));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("MARSocNh46xszubKP94ikV3awGLxQfiscx"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("MARSocNh46xszubKP94ikV3awGLxQfiscx"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseMarscoinURI(uri, &rv));
    QVERIFY(rv.address == QString("LQDPC5rbjDB72fGFVHu4enYhxGAZuRiFh9"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseMarscoinURI("marscoin://MARSocNh46xszubKP94ikV3awGLxQfiscx?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("LQDPC5rbjDB72fGFVHu4enYhxGAZuRiFh9"));
    QVERIFY(rv.label == QString());

    // We currently don't implement the message parameter (ok, yea, we break spec...)
    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?req-message=Wikipedia Example Address"));
    QVERIFY(!GUIUtil::parseMarscoinURI(uri, &rv));

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseMarscoinURI(uri, &rv));

    uri.setUrl(QString("marscoin:MARSocNh46xszubKP94ikV3awGLxQfiscx?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseMarscoinURI(uri, &rv));
}
