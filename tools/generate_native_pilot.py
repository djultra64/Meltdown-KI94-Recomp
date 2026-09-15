#!/usr/bin/env python3
"""Generate the finite KI native-pilot instruction switch ahead of time."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "native/src/generated/ki15d_native_pilot.inc"
IDENTITY_OUTPUT = ROOT / "native/src/generated/ki15d_native_pilot_identity.inc"
MANIFEST = ROOT / "provenance/native_pilot_manifest.json"
MANUAL = (0x880054D0, 0x880054EC, "provenance/asm/ki15d_880054d0.s",
          "5cd20496ebe2628e84587dbeff80c51806ef942a722a2d145156f479b2946fd4")
RANGES = (
    (0x88002060, 0x88002088, "provenance/asm/ki15d_88002060_native_pilot.s", "9489d8075e042a680e3c3efa76216e1fdb056697440f0bb5088b9ed332fbc569"),
    (0x88004E54, 0x88004E8C, "provenance/asm/ki15d_88004e54.s", "c4498c07620efaf50cfbee1bcab095ea540d173506b7eaf96fa13f2cdd327605"),
    (0x880045AC, 0x880045BC, "provenance/asm/ki15d_88004e54.s", "0d074f8785be1f9ecf3e65177f725b8fa7e9f7bfb1dfc2d99ad1ea1f54451a63"),
    (0x88004180, 0x88004188, "provenance/asm/ki15d_88004180.s", "815a642d3b2a75f931271ec78a9ed09cd8365fb4722ba02af85e22ecc070a45e"),
    (0x8800418C, 0x8800420C, "provenance/asm/ki15d_88004180.s", "a7689f6585dcd8ad07add046725450ff1c9890d909e20ee5e18762273c67550f"),
    (0x8800842C, 0x88008474, "provenance/asm/ki15d_8800842c.s", "86a3ddb73844ae9c9cde1c6709ed75a354d766aa4b79e81bd30a1eaecc49382b"),
    (0x88006670, 0x8800669C, "provenance/asm/ki15d_88006670_type1a.s", "9e000f0663f565519f9ac0272d4ddd408852068c91e75d757c11b1c924efcb40"),
    (0x88006788, 0x880067FC, "provenance/asm/ki15d_88006670_type1a.s", "208b238a0c74ce7be7347009ed659d6ae6bd1ceddd60bcb7a060f4f93759c9f1"),
    (0x88006C80, 0x88006CBC, "provenance/asm/ki15d_88006670_type1a.s", "50108c0f95e6c9aa83f209fd3cd3f5e30169a0337f27032898b66ffd54a9b850"),
    (0x88006D88, 0x88006D90, "provenance/asm/ki15d_88006670_type1a.s", "7c4a9c2c9f96d2021aa0567e37f254e10fbf799ae32f2595333d0792b01db1a3"),
    (0x88006314, 0x8800638C, "provenance/asm/ki15d_88006670_type1a.s", "1f4683ffa6002dc9a67c0aee0d6bf7ee9eee23b9b6f9b4f03f436fcdba6a7599"),
)
BINARY_RANGES = (
    (0x880001C4,0x880001EC,"855f53840283a2144b0dd564890c228a3893dac0c1986ef022c97a78fcaf601d"),
    (0x88000964,0x8800099C,"29dcf5e035c491298eaca49001848ebd77a0ea35ed92cde23bf086f8f8332f56"),(0x880009A4,0x880009AC,"e7909c11d1f35a7ce9527e47c02ed2b7a3372b2b7a48097a7ec9e98cd19bea65"),
    (0x88002038,0x88002060,"7fcf620c4e081f159c1d567a8b1c5289e4337b1f7dba8e4251449bdc786b7a8e"),(0x88002088,0x88002094,"1c42b468f1935359678235479bfdce66b0588b42c3f31514f2eb14c943ddfa2c"),
    (0x880021C8,0x88002218,"29c916ecff840701ca32e9c9fc4d7127a40d687bdf939575f4d1e2b36d4920b9"),(0x8800228C,0x8800229C,"7a0e0d3d5de3d8e0ebe76b5fbf4a4ae0a783e36321ebc1eb0290366d47624eaa"),
    (0x880022A8,0x880022B8,"c92c3c13ee98336a32bf2886cff4109942a2aaac375a5cf9316ec2c90acba85b"),(0x880022C4,0x880022D4,"41f66bea933392481534de60532f119222a44ee98a649e9279f0746a63ee1ab4"),
    (0x88002410,0x88002424,"9fa687b84fa39db58c4db6bd2ffc59db40c20e771b088462a6d9663d9ad0478f"),(0x8800271C,0x88002728,"ee0991a384e77efc8f46c08d7319199b211261dd157af947348f1f9db15af270"),
    (0x880027C4,0x880027D0,"aa9042e67b7bcb53f44a65202dc3feb45f2c46631cd2b87836127234857c3c43"),(0x88002870,0x880028B4,"d40b7e7d4c236ff8b31482eb8eb8099dcd24520d9e43b279f43cb44bb2b39026"),
    (0x880028C0,0x880028D0,"40ef26e7ffe0173de266d24820e53284319da6be36f0527bd2909c5b6385e421"),(0x88002904,0x88002940,"deeaa804c8b9a942bec04eea2f381eacf6d1ed1ec8f811de91053528378d24b3"),
    (0x88002994,0x880029A4,"696e4c17aebb0d9d39e4307b14ac8aef28adffc40261b1f17224ccf2c214e48f"),(0x880029F8,0x88002A00,"6ea53ea2df14d31945d87a5748f989e2bfe75a3aaa4315639ed39d4a98ea84f5"),
    (0x88002A24,0x88002A30,"90051f247bc32a6bd76b4b62a6c8e39990bbd7796f857e7ceaa9d0b289478a64"),(0x88002A6C,0x88002A7C,"835517bb695ef69f7d1378680c56e948b5c9250ae3bd14e2dfd4f6cf4b9e1be8"),
    (0x88004574,0x8800459C,"c8015d5530cb55233d6a2fbba1f7100c61986fda7096489f3dc9af550cef70c9"),(0x880054EC,0x88005540,"8f8c2e3339de970f05f00e9e844591fb33035342804d6d0d2581449b44c951b9"),
    (0x880055CC,0x88005608,"9d130c92c45d4f2d2cb39c2be5a0e2f79fe988e03c5b30d00c9d15f8d19e7485"),(0x88005620,0x88005628,"f3dc5c6ab1f87f4989ab62671ac1740c4b8e639c4b5250fe242043559cdca63d"),
    (0x88008528,0x880085CC,"a32213a1da54833c0af624b42a9e59e07671dd759edb28f227367fda32fe0693"),
)
RESUMED_RANGES = (
    (0x88002094, 0x880020E0, "fa7e3efec47b6ba7ac75d444a7e26f3bea8bc3d43e26ed176f50dbdc2291db7a"),
    (0x88002198, 0x880021C8, "3656e1e0a14b4679190af068eabe722cda5261d68edffeea58d0dee542d3a497"),
    (0x88002A7C, 0x88002AA0, "b1ffa1a6c655fa082cfebe202e616516a5a9953007e08981bd5810c7480ad01c"),
    (0x88002B04, 0x88002B48, "9228fef74615d12af9ed12032c05f5798b7f332dedd58a46a4b114d8a6ef102f"),
    (0x88002B88, 0x88002B94, "babb00f606646080dc52e20cde149f3c0aaff1a01d462600cfd53aa652fb1923"),
    (0x88002BF0, 0x88002C10, "cb72786232cef1d50c3a4acb1caf5d4593b8b9612244f8a10b01d6b17c2df5be"),
    (0x88002C44, 0x88002C50, "4bced7c33eb9292bfb3c44a5bfafe8b4195c4285a0600ab1e721fdc064b9fbf3"),
    (0x88002C54, 0x88002C88, "9dd1c816e282afd55fcdea6ab0a98ed88ab3a082d9c534577dfc02969858c4e1"),
    (0x88002C90, 0x88002C9C, "7b940f864fcb03cb7ffaed3c086a2f5883ebb3f6608287725b5ceea7f9d4237f"),
    (0x88002CA4, 0x88002CB0, "f22cf93735e5f83cbf450ba05de8e127f3da9a83ecf7b5cefb733f9ba0c4b981"),
    (0x88002CB8, 0x88002CC4, "98ef7b0e81edb4fee2c6007a91937dc58fdea56eb8bee53e7944138cd2c25424"),
    (0x88002CCC, 0x88002CD8, "f82e55b1f83165cfd66e30d3f190986c43430debba7a4aade815ee74c4e67fc4"),
    (0x88002CEC, 0x88002D20, "3702aef3be5a66cd14e67337e559150b4f1b0421279e75331e7b81e54f764d6d"),
    (0x88002D3C, 0x88002D58, "c61796d50aaa5bdbd7406ce7a576aabc5b1b10f657a3f22ff32c0280bcc274b2"),
    (0x88002D64, 0x88002D78, "ef919d9c8af80409e081b27fca04dc414c8a5a7c3ece4fb5b4d8fd5514347b0d"),
    (0x88002EB4, 0x88002EC4, "c1042ac02c707250d81586b738220f6811a545cf124011e226e13769a7a83c98"),
    (0x88003100, 0x88003120, "bd1d9fd8065df244bdcd236b0dcb43d0009b52463227a78518445a7a8d15f09a"),
    (0x88003120, 0x8800312C, "29393728d55ddbab59fe661311f7468da886b2242073893837a58ec53988fffc"),
    (0x88003294, 0x880032AC, "5e1ce20643b359454bffddae5a1e6eadd134861eabb53ab9005ac87f0c822454"),
    (0x880032D4, 0x880032FC, "621d9d06d3de1032f0403d5bf95ffc107023f2bc20af75240ef250b76019c2d2"),
    (0x88003304, 0x88003338, "58bd69a5949822c6a0c6bc8530b89b3cb70cdc2cef007346d88383f79a834d29"),
    (0x880033E0, 0x880033F0, "ea0b9406cc2b8a703245ea4c74bbb1100aadcaf11d0991d1909dc4171f6593f3"),
    (0x88003474, 0x88003484, "de0a8fcd6b6f5b51f037cef15a04d240bce1d69ea5e2c782ea1794f2fb63f7ff"),
    (0x880034C0, 0x880034CC, "14edde15cd69f38d970fcc0d6f1f4ce7ffb7a88886bd460de079fc9d39880cba"),
    (0x880034F0, 0x88003500, "c76f1665ef1d2f416b2fe2fddcb5b3a800830b215d1c6b7aac8e2804e518a9fd"),
    (0x88003554, 0x88003560, "bfd696d41b93084334af11e47af911aec6daed6614a00294850366d6fcd27493"),
    (0x88003568, 0x880035D4, "7352bc2239bbfa312b61d36d3128664b57cb46447b3bae00329da772bb46937e"),
    (0x880035D4, 0x880035E0, "fb44bf84cd4e5e2cc35c4299ee1aa2929598ac9f35022ac7697b8ed658e1e811"),
    (0x880036C8, 0x880036DC, "3415c6ee78cc3061ef303e3dbb3c59b0d8d69d9a847fe2a10389d69c12f5f3ed"),
    (0x8800377C, 0x880037AC, "cb8ebf6e108115b4d78eaa10adbba4ae6e6b73cbe7c03ea4ddfdd3c6ca68a876"),
    (0x880037BC, 0x880037DC, "c149d6c65ae90572109a091657a54ecffa03724f6ef06cc50693646f27f82fcf"),
    (0x8800384C, 0x8800386C, "c202fbfc9ef125c3765213a9e2b5e690a926aa30a4e9943fcc6aa14c728a3bfd"),
    (0x8800387C, 0x88003898, "98dea590e50b07ef6412c347a8a273df60f886cb6d26b66b0b918213ebb4af7e"),
    (0x88003904, 0x88003920, "3028acafcbe217523b188ac388b93ef011d552ca8c0abc543555027597be61aa"),
    (0x88003920, 0x8800392C, "cb485e127ed7dc9eb1df0a6f85c0e33a9be680596606ea57fa86e1652c1ba4bf"),
    (0x88003930, 0x88003938, "efe90262b22d55f0e8392dd3d4ea0390fc451bb02e08ea2fde41f93e113dc81e"),
    (0x88003BE4, 0x88003BEC, "55ee6c692af77062392ebc1c1d441a81f9d3a792b748447daabdadf43a421b72"),
    (0x88003C24, 0x88003C38, "b09540a88f3dc160e94100b06bf7f206f2dfba5db468e5057ff620066425d07a"),
    (0x88003C84, 0x88003C90, "6b7ff34d9785890ca144b85d07e207ad04084f87423d2912deb3480171b91be5"),
    (0x88003CC8, 0x88003CD4, "7d16a9a0254fe33ce57744df8c2602ab47d442b3c46115f85c299e5a84350686"),
    (0x88003CF0, 0x88003D00, "1e4383396311fd085d7f89010fecfe6e92fdc33b99d41e11f917973e5b8ab267"),
    (0x88003D30, 0x88003D64, "aca95dc6cbeb883910a80d0b0e97e7a11dbd65690961f8e4eeb581875cb5229b"),
    (0x88003D6C, 0x88003D74, "d4e17e5f22f96900a62f8eb324aabfb4b168cf25576e72d301201b603767e800"),
    (0x88004188, 0x8800418C, "2740ac36f6b55d5ada377d28a670e9aad470505c5f4c1c14771cf3000fb64779"),
    (0x88005880, 0x880058A0, "7ffe9e23ab86edb6faef5330a5825b18b06aa4f6f753671f62dc129f517ab8e0"),
    (0x8800590C, 0x88005960, "59b540426801c07779537f87129d9dd3cb8d9e225dade1b05b44068c5d0faa38"),
    (0x88005984, 0x88005994, "378af2b44c2721d35827eed378f525702bc154c301fa913ae2df41e4233a653f"),
    (0x880059A0, 0x880059C0, "e449d4ed572429284f66efd8e1d5fa0e0b36d032c54e38e32d811850c7bbd1d0"),
    (0x880059F0, 0x88005A18, "e77d14e249a8ece6b91a8118e31d79508d36cf631ef775e22d4de027e220fadb"),
    (0x88005A2C, 0x88005A70, "e60b9fd38fd3a29490c812272bbb89c52745b60c88f42a99544d43578375ae8b"),
    (0x88005A74, 0x88005A80, "1b8c8dad64fb2680f15bfdc4774e6fa9b4c7784f92fcefe47bac08022da0e48f"),
    (0x88005AAC, 0x88005AC4, "14e64863985d930f3c3deeb1e1d1f82adf71d3fe9c0f5f77a2fbf7d0b6bd1e49"),
    (0x88005ACC, 0x88005AEC, "0dda960726669204c743ccb3e4a60907425a6e217c4cbb284c34031a22161f46"),
    (0x88005B14, 0x88005B2C, "ff89dc30f814175f0604c0c4b0869741124d79ff9780d662f2f7884bcfa2c3a6"),
    (0x88005BC4, 0x88005BDC, "c6fe579a82d93bb7336ae7a6d358f35c88845e1fbe53c0e8062557abce9f6972"),
    (0x880085F4, 0x88008608, "21a2cf30ca037dfdf2d4a5cc39f7c9a0cc7f11337ffe9024b950dcc070ae5140"),
    (0x88008618, 0x8800863C, "6fe1c00c751b8c180a43f2d55ece0b9199531bba44cfbf6ffeae5b2c8c88afe7"),
    (0x88008754, 0x8800877C, "1c349424b79dddae3c6f42d31cc80bf9d059a0b4cd5644996b04aa167986cd9a"),
    (0x88008794, 0x880087B0, "81be3f118c491c44c9a3426a6f5a11ce2410fb05feaaf066ab6883ae2a44f2fa"),
    (0x880087C8, 0x88008828, "88a9768798e734b9e4446187f6031b097cd6143ba4fc25907f6ccf1b0994a826"),
    (0x88008914, 0x88008964, "97eb248ce8faddee93e0c17a40af09672bbc7779c5cdf1d558e2c2c174868268"),
    (0x88008A04, 0x88008A14, "3b6c114cee82e1ee624feb6c194519967585ee8ba2d49ccc14722a8e095fea01"),
    (0x88008B2C, 0x88008B64, "3f00e94e8f5b1e9b4bd8bc5e09ee2ae2e94aea83811b94395920a6989cc636cc"),
    (0x88008B78, 0x88008B84, "9fdcac194b0985e300d0e0d844042508bce92fb05e2499222e3bc5e005df8e8f"),
    (0x88008B90, 0x88008B9C, "1e8d2520f47a62d61ceb744b810a7482c970ae8f86b614188494ecb5804878aa"),
    (0x88008BA8, 0x88008BB0, "6d64edf91449c1b17746c1ef18afa2eb25c70bdf1322ab3df5a2630993b7e2f1"),
)
ALLOCATION_RANGES = (
    (0x88003D64, 0x88003D6C, "6813db4c036bc38b96a9a2ea4a92f18a7bd338e6b418f8fb8e5368ca8cab136f"),
    (0x8800B1D8, 0x8800B2D8, "5fe46c5f2277ed60816ba9c9aa0847dff43e7bf9471dd23d5fd857f3a3938349"),
    (0x880053F4, 0x88005430, "d8e8143226a28b209d201d5413a3d770b57f5210c8bc1b4a633a7b408f009eb1"),
    (0x880063AC, 0x880063EC, "8f0c43bc01e7ac68a1c7727610ab891957b41b11165a01ac4961a7b15d63d463"),
)
FRAME_RANGES = (
    (0x880016C4, 0x880016F4, "1fad9dc0a72c095c0771294982ae65a84b900de51373eafab79725708295db4b"),
    (0x880016F8, 0x88001708, "a5f68c0f5a0f1725b4397f93479686ebda70417476b9b52a03a9fd8d3f854d30"),
    (0x88001754, 0x880017E4, "56b2f38feaecbae8b739ce2f0021f8e83340ba3a39c8f7b99e1d116a1183e2f8"),
)
CONTACT_RANGE_TEXT = """
3d7c-3dc0 3dd8-3e14 3e24-3e7c 3e8c-3eec 3efc-3f64
53f4-5430 5bdc-5c08 6598-65fc 661c-6670
8cdc-8cec 8cf0-8d44 8da4-8e20 8e28-8e50 8e60-8f3c
8f9c-8fbc 8fd8-9074 9078-9084 9088-90a0 90a4-90ac
90b0-91b4 91b8-9274 934c-93a8 9428-9438 9448-9458
947c-9488 9498-9544 9548-9554 95cc-95e8 9734-975c
9760-97a0 97b0-97bc 97c8-97f4 97f8-982c 9830-988c
98d4-98ec 990c-992c 993c-9958 9968-9998 9a00-9a20
9a28-9a38 9a3c-9a4c 9a6c-9ad8 9adc-9ae8 9aec-9b2c
9b38-9b78 9b8c-9bb4 9bb8-9bcc 9bd0-9be4 9c04-9c24
9c3c-9c4c 9c50-9c8c 9c94-9ce8 9e98-9ec8 9f34-9f40
9f7c-9f98 a0d0-a108 a140-a164 a2c4-a2d0 a2e4-a33c
a340-a35c a36c-a388 a52c-a57c a584-a58c a598-a5dc
a5e4-a644 a664-a6f8 a710-a72c
"""
CONTACT_RANGES = tuple((0x88000000 + int(item.split("-")[0], 16),
                        0x88000000 + int(item.split("-")[1], 16))
                       for item in CONTACT_RANGE_TEXT.split())
CONTACT_WORDS_SHA256 = "cf656d5d8905002ef3cbcea2d819d121b862ed1286cf159756937a243ddf0c3d"
RENDER_RANGES = (
 (0x88001b90,0x88001bb0,"8239046b41b1a9d6f108ac102d5a10223f2c5584dffa8ea599e2624e32d64e55"),(0x88001bb4,0x88001bd4,"29d187d7b526e2fe97ad0e13c23983d3587cf71fc6d6bb7ad96c919464ba1501"),(0x88001bf8,0x88001c80,"abbc2e9a048057f4ab302d38685041a4390f9c945c3d1863c3e61a8fe60b5e03"),
 (0x88001e0c,0x88001e1c,"791aec67a17fcad8e5a9d5d778767c06fc8c6394edf58bd0feea9673d3428414"),(0x88001e44,0x88001e78,"06070ba52f8209ae4731d26d943bc4f45e0129e3f05dfae41621dff1ae5b148c"),(0x88001ffc,0x88002038,"88fc436c6afe9d0dd5fafdd7499881f673dda648a653faf5095ba8bf69602b1f"),
 (0x880107a8,0x88010804,"f02802b2eee93e46b3734383dd0fb00493b94339825fdd14e19275b00a29bae6"),(0x88010814,0x88010820,"463f5021fd77e114cd21985e3362546533aaa3a14aa1927d170b2bf51bd83674"),(0x88010824,0x88010834,"3486d77a1b7dfe352cb4457897fdf0999da08ef4b886b87f745740f6b304269b"),(0x88010838,0x88010844,"c07803eb6686c73216a6e82069dd701b45982568cf3e8d604b43fbcd0311db19"),(0x88010898,0x880108a0,"2c75fdcdbb47de855900b3b693c25a862caf7a7d50915eac6be2bd6f12769094"),(0x880108b8,0x880108cc,"b6fd93a678b1f650a318182e9d03ec8d502f0269ab3878b1490676696ae5c57b"),(0x880108d0,0x880108ec,"a5702e3fd94ae320ef3bf583f6799a1e2ec06d031d97ad832ce8d070c0350b91"),(0x880108f0,0x88010978,"581f7e98e34f9674b7b72b83e039b918071f4aa00ebc2f31158316c4fdb2f5b2"),
 (0x88010a30,0x88010a64,"a069c40385474b924171dba8debf4c4eab43543e1687329aa666884d23e58a48"),(0x88010a68,0x88010a80,"77745610cf494854a69146af07b8dbdc1ec6b55cfae8e272c806c52903ebad5b"),(0x88010adc,0x88010b14,"4eb77be40e5c35bb041ad9dee7d6f5562f4350eb0545c1500bcae580faa9b355"),(0x88010b44,0x88010b68,"999c551b3c6b1c2f9f98ed75e3c5e37678c2001c87ccff293fa3b21ebed2ce17"),(0x88010b6c,0x88010b84,"60efd27b3455a562966984b1815c9f1184d4cae8a17ceda460d630e9a525a153"),(0x88010bd4,0x88010c04,"3264dfb53e1d16603e77d081088547c9998c6278c0da6be3e3875a09817e08a6"),(0x88010c08,0x88010c14,"39f95a069f0078ea3823ae130ead9ce5ec60e653e4c25cf1739fd5f5434be173"),(0x88010cbc,0x88010d00,"ecdcce21a39731e9e15f3277012f5987451eceec75a9de2f99efb53d8adaab11"),(0x88010d0c,0x88010d1c,"3082dcb585b981563a11abbc7a4427b4d5e71922b0a0034c561027efd8bc4314"),(0x8801180c,0x880118c0,"1fbd5be699b1d18e0ad47c9aa73f4fc20832748393f8051b6e828d29648ed0b3"),(0x88011918,0x88011978,"dea23c5549c06ce137fb283ccde32f5040b8ad54a5667aac84191ca0f1229669")
)
PROJECTION_RANGES = (
 (0x88001024,0x88001194,"7975ddfb7769b6ce36a7517cf77582818c0fe4931dabe513258db3f8ed4dc96c"),(0x880011c4,0x880011e4,"b95557be158a54bb0d3d16f05e07e070ff89cec4819909a8ab11ec5a8b6bf4f2"),(0x880011ec,0x880011f8,"1018a4a7da7a2f700f0c285ad6133905afb1fb372b9f6000d17b897537699e6a"),(0x8800120c,0x88001238,"d0ce1b27021f9a0548fa2dc3943541e65790fc595366a4ff921a83634d164918"),
 (0x8800d390,0x8800d3ac,"07dd6401342d34e2968bd9c5dcef069fc2487dbb1fef13410430e78d96d5aa6c"),(0x8800d4c4,0x8800d4e0,"f7625ab767854dc0d51c0ebcb84d48483deed35669ecbcb971408e8293272384"),
 (0x88019d50,0x88019d6c,"8ca7c0f09965f434aa6dcd2226451286c9a579b94ffa593055f9f8890d726711"),(0x88019d7c,0x88019d90,"35f3296762d194adfc57aa77f5b2176b233026b24f7a11b464d2ad9795e427c3"),(0x88019d94,0x88019dac,"8e855fb622386fb5b46af7906caedbbaf728957719c16931a80df4751437805d"),(0x88019db4,0x88019e10,"54b32530878b7338b6c85e52591a8f5c72553a606927f04107fd5e48a1b8c348"),(0x88019e14,0x88019e3c,"c7f28996f9375eba6852f9895da3d36ab869d44cd7372c5b2e8e22292c91e19e"),(0x88019e44,0x88019e50,"a0c9c4a140dbd9c923dab6698fbb7c1c78003cdf007e2feca2c14f806bdd53f2"),(0x88019e60,0x88019e74,"9819c04d8b8a390f48b3f9328fc5bd9229916c25cf1f74593e621519b019fcf9"),(0x88019e7c,0x88019e90,"d068738145a9393acec1a744e33a9a7557bb663eba241a4b8dc0dc6667871f95"),(0x88019e94,0x88019ee0,"d780e8575049c80008b874b74ad6fe7bb189b1246b1a02fc70c8e0619da84400"),(0x88019ee4,0x88019eec,"7956458a2f579d65b8ddadd90164e618ad88b51ac0862403f651499a39ed0253"),(0x88019f44,0x8801a1e8,"3fe8fdd4eb1886944e6f32791aad771a94c115ac604821d16a114e068ed9d689"),
)
PUBLISHER_RANGES = (
    (0x88006944, 0x880069D4,
     "1bbd76cbdd4ee425762170b03d94b85b79026122d998cbe1310bf8ab2db43b99"),
)
TYPE12_RANGES = (
 (0x8800464c,0x88004678,"3c07f3daf133b1e38e89d5caadc9c274e9c01fb35e0206ac11e58df9da7d7220"),(0x8800468c,0x880046b0,"566b2a30b47cd78b920d40c7d24b30badc98f38f0893c926e18b9b1baaf39904"),(0x880046d0,0x880046e4,"891d6fea786813b45f2ebe88c2b15cc160314f067bb453f0f187c72c0c40165a"),(0x88004878,0x88004978,"eb82aed7d99e23da9801e9403c6e08aaca41780e76251a89779845563ae7d93c"),(0x88004984,0x88004994,"f76051e2c1199bd13a289525522e0efc9aea30e689a81ad5d180468c08169491"),(0x880049c8,0x880049d4,"e5f33e3c43ace76c94dd846caf5ca4e850b65df30d54a63f00293e9aaf21f719"),(0x88004a08,0x88004a20,"2ad7e3cdcaa19713b49c7af48b3662548e0f2bd21df626e5c9e294fd78d23f98"),(0x88004a64,0x88004a80,"8980b4265e0f7689ef276b937a63bcb12d001031befd678c1395b84be213472b"),(0x88004bbc,0x88004bc8,"f3f68be79fb5be6c8a1ecd28e762b59f352c559eda6afa96dd5905dc335a5ea4"),(0x88004c44,0x88004c58,"9ff98a8d08eba8d9cc7164ad18c4619d0a6feb917c68b5e0d0de37d16306e8bf"),(0x88004c88,0x88004c98,"0e9c354ccfb9a0922cdb05db19533aeb3a81480429bfea1549c8d9fb3ce3e638"),(0x8800a7e8,0x8800a814,"11eac2fabca0a12934f1a217ca8b8ba5cb0be7cef04614233ca81482347a2565"),(0x8800a950,0x8800a96c,"022875d00aafc9de028d84fd75f4a01ca4715cb86761740584931ddb32554fcf"),
)
CONNECTED_SPINE_RANGES = (
 (0x8800099c,0x880009f0,"876655bbb8113052d0c1cd05f86da19254b19d5fc2051da2897b924237f44891"),(0x88000a54,0x88000aa4,"12052c9dae2247c37de80fc38ff7a406f63f8082351e56bb96c0abef1ace076b"),(0x88000c10,0x88000cb4,"a602526761d2b1a9a9fa3e2b667907bdd114360c3abdb7456143373959f18eef"),(0x88000cdc,0x88000d60,"2d1662f4357af4e2595b5d3f600de4c3ad6f1be7b334df9f0271feac0713dd33"),(0x88000d60,0x88000d78,"d9e04d0da4f9dc83a349183f46d7051b9bdf934b50dad7b67b83c0d6d5f0d14b"),(0x88000da8,0x88000e3c,"f7cd60f26f8f67104257bfffb3629124c70ae65b012e90dc961343689e480362"),(0x88000ed0,0x88000f40,"cf845e8cf2643ba7513c503b3ca26a169060c54ecdd243d6fe00707d7f648ff4"),(0x88000f70,0x88001038,"160c7ae07e3be43a129704a130f4db6d749140983846ce681fc9992ad06cc72b"),(0x88001238,0x880012b8,"f7e9003aaf8b0af495cbb4b1f034470a140c031eed68f755859c16cef7833f59"),(0x8800d05c,0x8800d078,"9f4d06bf14d6a5ec81c290f5734fd02ef572d937a769cb764f96d09d83ef44e0"),
 (0x88008cbc,0x88008cdc,"1c718714ad5a249bc409108ae739fadfad4ff23981cf43029c20581dd92cc379"),(0x88008cec,0x88008cf0,"3d36e6d3ebeae41f616e43b2a7183975c65dcf9e28ea48931cec1df012c6eee5"),(0x88008fbc,0x88008fd8,"14c8d7ad57c86a7d08bc2cc5e6982db6b5b755cb871d05478079a4f20efcccf2"),(0x880011e4,0x880011ec,"d0ae54b5ca8ddc1f25b710b5972721e2edb6ac2a018c7fa4d197ac9a9141bc11"),(0x88008e20,0x88008e28,"36f7c5fed804e6992006d4e3ff12cb0fc9f905688ccfd4607d9552b814dfc3c6"),(0x88009074,0x88009078,"302f6149afe2cae5e9fccd3fd9a271b5912b8c9b04a5271ad0cb0d1c0aaa194c"),(0x88009084,0x88009088,"94ba2d5f7d922ba57f8e11fc91fbdd71e0a718e02bc9a9ec4278bc7d62ad7cce"),(0x880090a0,0x880090a4,"2932cbec443e0a38725f732c36c0846ba8be518a4dcd15628dce12124c5ed533"),(0x880090ac,0x880090b0,"feb48fa571b77ac70a3f93878cddd1267eb0c2607d51e5af3b88bb78fdecb721"),(0x8800a72c,0x8800a740,"2312d178c3866e9d995585f98b4da9cfcb28138502b20d0e644a90ad0e47a2d9"),
)
SOURCE_DATA = (
    (0x8800B2D8, bytes.fromhex("0c"), "ef6cbd2161eaea7943ce8693b9824d23d1793ffb1c0fca05b600d3899b44c977"),
    (0x8805E370, bytes.fromhex("f2e605880a0a0400"), "eab759ba801843ccbb5890c4d0260151a84b16f73f27a17293af76253f6660d5"),
    (0x8805E6F2, bytes.fromhex("001001000402"), "e23118c4c89399721771521fb980abb1d03f6c7c4f573e327b9e1fe6e8b8712f"),
    (0x8804233C, bytes.fromhex("0004f4411505f809f7f116fc"),
     "d228b63a0be23dbdcebb95985a726f703955c297ddf02efaf17dcecd86367d16"),
    (0x88042384, bytes.fromhex("0004f4411505f809f7f116fc"),
     "d228b63a0be23dbdcebb95985a726f703955c297ddf02efaf17dcecd86367d16"),
)
LINE = re.compile(r"^([0-9a-fA-F]{8}):\s+([0-9a-fA-F]{8})\b")


class GenerationError(ValueError):
    pass


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def load_words() -> list[tuple[int, int]]:
    parsed: dict[str, dict[int, int]] = {}
    result = []
    for start, end, relative, expected_hash in RANGES:
        if relative not in parsed:
            words = {}
            for line in (ROOT / relative).read_text(encoding="utf-8").splitlines():
                match = LINE.match(line)
                if match:
                    address = int(match.group(1), 16)
                    if address in words:
                        raise GenerationError(f"duplicate source address 0x{address:08x}")
                    words[address] = int(match.group(2), 16)
            parsed[relative] = words
        selected = [(address, parsed[relative].get(address))
                    for address in range(start, end, 4)]
        if any(word is None for _, word in selected):
            raise GenerationError(f"missing instruction in {relative} for 0x{start:08x}..0x{end:08x}")
        data = b"".join(struct.pack("<I", int(word)) for _, word in selected)
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise GenerationError(f"source words changed for 0x{start:08x}..0x{end:08x}")
        result.extend((address, int(word)) for address, word in selected)
    segment = (ROOT / "work/kipack/ki15d/rom-0.bin").read_bytes()
    for start, end, expected_hash in BINARY_RANGES + RESUMED_RANGES + ALLOCATION_RANGES + FRAME_RANGES + RENDER_RANGES + PROJECTION_RANGES + PUBLISHER_RANGES + TYPE12_RANGES + CONNECTED_SPINE_RANGES:
        first, last = start - 0x88000000, end - 0x88000000
        data = segment[first:last]
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise GenerationError(f"binary source words changed for 0x{start:08x}..0x{end:08x}")
        result.extend((0x88000000 + offset, struct.unpack_from("<I", segment, offset)[0])
                      for offset in range(first, last, 4))
    contact = []
    for start, end in CONTACT_RANGES:
        contact.extend((address, struct.unpack_from("<I", segment, address-0x88000000)[0])
                       for address in range(start, end, 4))
    contact_data = b"".join(struct.pack("<I", word) for _, word in sorted(contact))
    if len(contact) != 1000 or hashlib.sha256(contact_data).hexdigest() != CONTACT_WORDS_SHA256:
        raise GenerationError("contact source selection changed")
    result.extend(contact)
    if len(result) != 3209 or len({address for address, _ in result}) != 3187:
        raise GenerationError("finite enrollment must contain exactly 3187 unique words")
    return sorted(dict(result).items())


def load_manual_words() -> list[tuple[int, int]]:
    start, end, relative, expected_hash = MANUAL
    words = {}
    for line in (ROOT / relative).read_text(encoding="utf-8").splitlines():
        match = LINE.match(line)
        if match:
            words[int(match.group(1), 16)] = int(match.group(2), 16)
    selected = [(address, words.get(address)) for address in range(start, end, 4)]
    if any(word is None for _, word in selected):
        raise GenerationError("manual adapter source range is incomplete")
    data = b"".join(struct.pack("<I", int(word)) for _, word in selected)
    if hashlib.sha256(data).hexdigest() != expected_hash:
        raise GenerationError("manual adapter source words changed")
    return [(address, int(word)) for address, word in selected]


def render_identity() -> str:
    digest = hashlib.sha256(MANIFEST.read_bytes()).digest()
    values = ",".join(f"0x{value:02x}" for value in digest)
    return ("/* SHA-256 of provenance/native_pilot_manifest.json exact bytes. */\n"
            f"static const uint8_t snapshot_identity[32] = {{{values}}};\n")


def emit_instruction(pc: int, word: int) -> str:
    op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    shift, funct, imm = (word >> 6) & 31, word & 63, word & 0xFFFF
    simm = signed16(imm)
    branch = (pc + 4 + simm * 4) & 0xFFFFFFFF
    target = ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
    if word == 0:
        return "NOP();"
    if op == 0:
        table = {
            0x00: f"SLL({rd}, {rt}, {shift});",
            0x02: f"SRL({rd}, {rt}, {shift});",
            0x03: f"SRA({rd}, {rt}, {shift});",
            0x08: f"JR({rs});",
            0x0D: f"BREAK(0x{word >> 6 & 0xfffff:05x}u);",
            0x10: f"MFHI({rd});",
            0x12: f"MFLO({rd});",
            0x18: f"MULT({rs}, {rt});",
            0x1A: f"DIV({rs}, {rt});",
            0x1B: f"DIVU({rs}, {rt});",
            0x21: f"ADDU({rd}, {rs}, {rt});",
            0x22: f"SUB({rd}, {rs}, {rt});",
            0x23: f"SUBU({rd}, {rs}, {rt});",
            0x24: f"AND({rd}, {rs}, {rt});",
            0x25: f"OR({rd}, {rs}, {rt});",
            0x26: f"XOR({rd}, {rs}, {rt});",
            0x2A: f"SLT({rd}, {rs}, {rt});",
        }
        if funct in table:
            return table[funct]
    simple = {
        0x02: f"JUMP(0x{target:08x}u);",
        0x03: f"JAL(0x{target:08x}u);",
        0x04: f"BRANCH(G({rs}) == G({rt}), 0x{branch:08x}u);",
        0x05: f"BRANCH(G({rs}) != G({rt}), 0x{branch:08x}u);",
        0x06: f"BRANCH(SIGNED_LE_ZERO(G({rs})), 0x{branch:08x}u);",
        0x07: f"BRANCH(SIGNED_GT_ZERO(G({rs})), 0x{branch:08x}u);",
        0x09: f"ADDIU({rt}, {rs}, {simm});",
        0x0A: f"SLTI({rt}, {rs}, {simm});",
        0x0C: f"ANDI({rt}, {rs}, 0x{imm:04x}u);",
        0x0D: f"ORI({rt}, {rs}, 0x{imm:04x}u);",
        0x0E: f"XORI({rt}, {rs}, 0x{imm:04x}u);",
        0x0F: f"LUI({rt}, 0x{imm:04x}u);",
        0x21: f"LH({rt}, {rs}, {simm});",
        0x20: f"LB({rt}, {rs}, {simm});",
        0x23: f"LW({rt}, {rs}, {simm});",
        0x24: f"LBU({rt}, {rs}, {simm});",
        0x25: f"LHU({rt}, {rs}, {simm});",
        0x28: f"SB({rt}, {rs}, {simm});",
        0x29: f"SH({rt}, {rs}, {simm});",
        0x2B: f"SW({rt}, {rs}, {simm});",
        0x31: f"LWC1({rt}, {rs}, {simm});",
        0x39: f"SWC1({rt}, {rs}, {simm});",
        0x37: f"LD({rt}, {rs}, {simm});",
        0x3F: f"SD({rt}, {rs}, {simm});",
    }
    if op == 0x01 and rt in (0, 1):
        condition = f"SIGNED_LT_ZERO(G({rs}))" if rt == 0 else f"!SIGNED_LT_ZERO(G({rs}))"
        return f"BRANCH({condition}, 0x{branch:08x}u);"
    if op == 0x10 and rs == 0:
        return f"MFC0({rt}, {rd});"
    if op == 0x10 and rs == 4:
        return f"MTC0({rt}, {rd});"
    if op == 0x11 and rs == 0:
        return f"MFC1({rt}, {rd});"
    if op == 0x11 and rs == 4:
        return f"MTC1({rt}, {rd});"
    if op == 0x11 and rs == 8 and rt in (0, 1):
        return f"FP_BRANCH({'false' if rt == 0 else 'true'}, 0x{branch:08x}u);"
    if op == 0x11 and rs == 16 and funct in (0, 1, 2, 3):
        names = ("ADD_S", "SUB_S", "MUL_S", "DIV_S")
        return f"{names[funct]}({shift}, {rd}, {rt});"
    if op == 0x11 and rs == 16 and funct == 0x04:
        return f"SQRT_S({shift}, {rd});"
    if op == 0x11 and rs == 16 and funct == 0x05:
        return f"ABS_S({shift}, {rd});"
    if op == 0x11 and rs == 16 and funct == 0x06:
        return f"MOV_S({shift}, {rd});"
    if op == 0x11 and rs == 16 and funct == 0x07:
        return f"NEG_S({shift}, {rd});"
    if op == 0x11 and rs == 16 and funct == 0x24:
        return f"CVT_W_S({shift}, {rd});"
    if op == 0x11 and rs == 16 and funct == 0x34:
        return f"C_OLT_S({rd}, {rt});"
    if op == 0x11 and rs == 20 and funct == 0x20:
        return f"CVT_S_W({shift}, {rd});"
    if op in simple:
        return simple[op]
    raise GenerationError(f"unsupported word 0x{word:08x} at 0x{pc:08x}")


def render(words: list[tuple[int, int]]) -> str:
    lines = ["/* Generated by tools/generate_native_pilot.py; do not edit. */",
             "static const KiPilotSourceWord generated_words[] = {"]
    lines += [f"    {{UINT32_C(0x{pc:08x}), UINT32_C(0x{word:08x})}},"
              for pc, word in words]
    lines += ["};", "", "static const KiPilotSourceByte source_data_bytes[] = {"]
    for address,data,expected_hash in SOURCE_DATA:
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise GenerationError(f"source data declaration changed at 0x{address:08x}")
        offset=address-0x88000000
        if offset < 0x33900:
            actual=(ROOT/"work/kipack/ki15d/rom-0.bin").read_bytes()[offset:offset+len(data)]
        else:
            actual=(ROOT/"work/kipack/ki15d/rom-1.bin").read_bytes()[offset-0x33900:offset-0x33900+len(data)]
        if actual != data:
            raise GenerationError(f"source data image changed at 0x{address:08x}")
        lines += [f"    {{UINT32_C(0x{address+index:08x}), UINT8_C(0x{value:02x})}},"
                  for index,value in enumerate(data)]
    lines += ["};", "", "static const KiPilotSourceWord manual_source_words[] = {"]
    lines += [f"    {{UINT32_C(0x{pc:08x}), UINT32_C(0x{word:08x})}},"
              for pc, word in load_manual_words()]
    lines += ["};", "",
             "static bool generated_execute(KiNativePilot *pilot)", "{",
             "    const bool apply_delay = pilot->cpu.delay_pending;",
             "    const uint64_t delayed_target = pilot->cpu.delay_target;",
             "    pilot->cpu.delay_pending = false;",
             "    const uint64_t sequential_pc = (uint32_t)pilot->cpu.pc + 4u;",
             "    switch ((uint32_t)pilot->cpu.pc) {"]
    for pc, word in words:
        lines.append(f"    case 0x{pc:08x}u: /* 0x{word:08x} */ {emit_instruction(pc, word)} break;")
    lines += ["    default: return pilot_stop(pilot, KI_PILOT_STOP_UNKNOWN_PC, pilot->cpu.pc, 0);",
              "    }", "    pilot->cpu.gpr[0] = 0;",
              "    pilot->cpu.pc = apply_delay ? delayed_target : sequential_pc;",
              "    ++pilot->instruction_count;", "    return true;", "}", ""]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        generated = render(load_words())
        generated_identity = render_identity()
        output = args.output if args.output.is_absolute() else ROOT / args.output
        if args.check:
            if output.read_text(encoding="utf-8") != generated:
                raise GenerationError(f"stale generated output: {output}")
            if IDENTITY_OUTPUT.read_text(encoding="utf-8") != generated_identity:
                raise GenerationError(f"stale generated output: {IDENTITY_OUTPUT}")
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(generated, encoding="utf-8")
            IDENTITY_OUTPUT.write_text(generated_identity, encoding="utf-8")
    except (OSError, GenerationError) as exc:
        parser.exit(1, f"native pilot generation error: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
