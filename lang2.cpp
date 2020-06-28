#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

static std::unordered_map<int, std::string> MacintoshLanguageCodes{
    {0, "English"},       {1, "French"},
    {2, "German"},        {3, "Italian"},
    {4, "Dutch"},         {5, "Swedish"},
    {6, "Spanish"},       {7, "Danish"},
    {8, "Portuguese"},    {9, "Norwegian"},
    {10, "Hebrew"},       {11, "Japanese"},
    {12, "Arabic"},       {13, "Finnish"},
    {14, "Greek"},        {15, "Icelandic"},
    {16, "Maltese"},      {17, "Turkish"},
    {18, "Croatian"},     {19, "Traditional Chinese"},
    {20, "Urdu"},         {21, "Hindi"},
    {22, "Thai"},         {23, "Korean"},
    {24, "Lithuanian"},   {25, "Polish"},
    {26, "Hungarian"},    {27, "Estonian"},
    {28, "Latvian"},      {29, "Sami"},
    {30, "Faroese"},      {31, "Farsi"},
    {32, "Russian"},      {33, "Simplified Chinese"},
    {34, "Flemish"},      {35, "Irish"},
    {36, "Albanian"},     {37, "Romanian"},
    {38, "Czech"},        {39, "Slovak"},
    {40, "Slovenian"},    {41, "Yiddish"},
    {42, "Serbian"},      {43, "Macedonian"},
    {44, "Bulgarian"},    {45, "Ukrainian"},
    {46, "Belarusian"},   {47, "Uzbek"},
    {48, "Kazakh"},       {49, "Azerbaijani"},
    {50, "AzerbaijanAr"}, {51, "Armenian"},
    {52, "Georgian"},     {53, "Moldavian"},
    {54, "Kirghiz"},      {55, "Tajiki"},
    {56, "Turkmen"},      {57, "Mongolian"},
    {58, "MongolianCyr"}, {59, "Pashto"},
    {60, "Kurdish"},      {61, "Kashmiri"},
    {62, "Sindhi"},       {63, "Tibetan"},
    {64, "Nepali"},       {65, "Sanskrit"},
    {66, "Marathi"},      {67, "Bengali"},
    {68, "Assamese"},     {69, "Gujarati"},
    {70, "Punjabi"},      {71, "Oriya"},
    {72, "Malayalam"},    {73, "Kannada"},
    {74, "Tamil"},        {75, "Telugu"},
    {76, "Sinhala"},      {77, "Burmese"},
    {78, "Khmer"},        {79, "Lao"},
    {80, "Vietnamese"},   {81, "Indonesian"},
    {82, "Tagalog"},      {83, "MalayRoman"},
    {84, "MalayArabic"},  {85, "Amharic"},
    {86, "Galla"},        {87, "Oromo"},
    {88, "Somali"},       {89, "Swahili"},
    {90, "Kinyarwanda"},  {91, "Rundi"},
    {92, "Nyanja"},       {93, "Malagasy"},
    {94, "Esperanto"},    {128, "Welsh"},
    {129, "Basque"},      {130, "Catalan"},
    {131, "Latin"},       {132, "Quechua"},
    {133, "Guarani"},     {134, "Aymara"},
    {135, "Tatar"},       {136, "Uighur"},
    {137, "Dzongkha"},    {138, "JavaneseRom"},
};

static const char before[][4] = {
    /* 0-9 */
    "eng", "fra", "ger", "ita", "dut", "sve", "spa", "dan", "por", "nor", "heb",
    "jpn", "ara", "fin", "gre", "ice", "mlt", "tur", "hr " /*scr*/,
    "chi" /*ace?*/, "urd", "hin", "tha", "kor", "lit", "pol", "hun", "est",
    "lav", "", "fo ", "", "rus", "chi", "", "iri", "alb", "ron", "ces", "slk",
    "slv", "yid", "sr ", "mac", "bul", "ukr", "bel", "uzb", "kaz", "aze",
    /*?*/
    "aze", "arm", "geo", "mol", "kir", "tgk", "tuk", "mon", "", "pus", "kur",
    "kas", "snd", "tib", "nep", "san", "mar", "ben", "asm", "guj", "pa ", "ori",
    "mal", "kan", "tam", "tel", "", "bur", "khm", "lao",
    /*                   roman? arabic? */
    "vie", "ind", "tgl", "may", "may", "amh", "tir", "orm", "som", "swa",
    /*==rundi?*/
    "", "run", "", "mlg", "epo", "", "", "", "", "",
    /* 100 */
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "wel", "baq", "cat", "lat", "que",
    "grn", "aym", "tat", "uig", "dzo", "jav"};

static const char after[][4] = {
    /*  0: [English] [French] [German] [Italian] [Dutch] [Swedish] [Spanish]
       [Danish] [Portuguese] [Norwegian] */
    "eng", "fra", "deu", "ita", "nld", "swe", "spa", "dan", "por", "nor",
    /* 10: [Hebrew] [Japanese] [Arabic] [Finnish] [Greek] [Icelandic] [Maltese]
       [Turkish] [Croatian] [Traditional Chinese] */
    "heb", "jpn", "ara", "fin", "gre", "isl", "mlt", "tur", "hrv", "zho",
    /* 20: [Urdu] [Hindi] [Thai] [Korean] [Lithuanian] [Polish] [Hungarian]
       [Estonian] [Latvian] [Sami] */
    "urd", "hin", "tha", "kor", "lit", "pol", "hun", "est", "lav", "",
    /* 30: [Faroese] [Farsi/Persian] [Russian] [Simplified Chinese] [Flemish]
       [Irish] [Albanian] [Romanian] [Czech] [Slovak] */
    "fao", "fas", "rus", "zho", "nld", "gle", "sqi", "ron", "ces", "slk",
    /* 40: [Slovenian] [Yiddish] [Serbian] [Macedonian] [Bulgarian] [Ukrainian]
       [Belarusian] [Uzbek] [Kazakh] [Azerbaijani] */
    "slv", "yid", "srp", "mkd", "bul", "ukr", "bel", "uzb", "kaz", "aze",
    /* 50: [AzerbaijanAr] [Armenian] [Georgian] [Moldavian] [Kirghiz] [Tajiki]
       [Turkmen] [Mongolian] [MongolianCyr] [Pashto] */
    "aze", "hye", "kat", "mol", "kir", "tgk", "tuk", "mon", "mon", "pus",
    /* 60: [Kurdish] [Kashmiri] [Sindhi] [Tibetan] [Nepali] [Sanskrit] [Marathi]
       [Bengali] [Assamese] [Gujarati] */
    "kur", "kas", "snd", "bod", "nep", "san", "mar", "ben", "asm", "guj",
    /* 70: [Punjabi] [Oriya] [Malayalam] [Kannada] [Tamil] [Telugu] [Sinhala]
       [Burmese] [Khmer] [Lao] */
    "pan", "ori", "mal", "kan", "tam", "tel", "sin", "mya", "khm", "lao",
    /* 80: [Vietnamese] [Indonesian] [Tagalog] [MalayRoman] [MalayArabic]
       [Amharic] [Galla] [Oromo] [Somali] [Swahili] */
    "vie", "ind", "tgl", "may", "may", "amh", "tir", "orm", "som", "swa",
    /* 90: [Kinyarwanda] [Rundi] [nya] [Malagasy] [Esperanto]  */
    "kin", "run", "nya", "mlg", "epo", "", "", "", "", "",
    /* 100: */
    "", "", "", "", "", "", "", "", "", "",
    /* 110: */
    "", "", "", "", "", "", "", "", "", "",
    /* 120:                                                 [Welsh] [Basque] */
    "", "", "", "", "", "", "", "", "cym", "eus",
    /* 130: [Catalan] [Latin] [Quechua] [Guarani] [Aymara] [Tatar] [Uighur]
       [Dzongkha] [JavaneseRom] */
    "cat", "lat", "que", "grn", "aym", "tat", "uig", "dzo", "jav"};

int main() {
    for (unsigned i = 0; i < sizeof(before) / sizeof(before[0]); i++) {
        std::string str = "\"";
        str += before[i];
        str += "\"";
        const char *lan_name = MacintoshLanguageCodes[i].c_str();
        printf("%5s,    /* %3u %s */ \n", str.c_str(), i, lan_name);
    }

    printf("====================\n");
    for (unsigned i = 0; i < sizeof(after) / sizeof(after[0]); i++) {
        std::string str = "\"";
        str += after[i];
        str += "\"";
        const char *lan_name = MacintoshLanguageCodes[i].c_str();
        printf("%5s,    /* %3u %s */ \n", str.c_str(), i, lan_name);
    }

    return 0;
}
