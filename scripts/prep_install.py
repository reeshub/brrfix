# brrfix's own small version of the development repository's prep_install.py: only what the build
# scripts here use. Written by that repository's brrfix-sync; don't edit it here.
"""The three files the 1996 installer writes into the game folder, for setup_build."""

import base64
import datetime

INSTALL_TIME = datetime.datetime(1996, 3, 18, 15, 41, 0)
FILES = {
    'DEFAULT.CFG': 'Ow0KO0JpZyByZWQgcmFjaW5nIGNvbmZpZyBmaWxlDQo7DQoNCm1hbmljIDENCmxhbmcgRU5HDQp2b2xzIDQsNA0K',
    'DIG.INI': 'Ow0KO01pbGVzIERlc2lnbiBBdWRpbyBJbnRlcmZhY2UgTGlicmFyeSBWMy4wMiBvZiAxOC1KYW4tOTUNCjsNCg0KREVWSUNFICAgICAgQ3JlYXRpdmUgTGFicyBTb3VuZCBCbGFzdGVyIDE2IG9yIEFXRTMyDQpEUklWRVIgICAgICBTQjE2LkRJRw0KSU9fQUREUiAgICAgMjIwaA0KSVJRICAgICAgICAgLTENCkRNQV84X0JJVCAgIC0xDQpETUFfMTZfQklUICAtMQ0K',
    'VIDEO.BAT': 'QGVjaG8gb2ZmDQpAdmlkdGh1bXAgRDpcdmlkZW9caW50cm9zbmQucnBsDQo=',
}


def generated_files():
    return {name: base64.b64decode(data) for name, data in FILES.items()}
