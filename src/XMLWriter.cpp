#include "XMLWriter.h"

#include <string>
#include <vector>

static std::string EscapeText(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

static std::string EscapeAttr(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else if (c == '\'') out += "&apos;";
        else out += c;
    }
    return out;
}

struct CXMLWriter::SImplementation {
    std::shared_ptr<CDataSink> DSink;
    std::vector<std::string> DOpen;

    SImplementation(std::shared_ptr<CDataSink> sink) : DSink(sink) {}

    bool WriteString(const std::string &s) {
        std::vector<char> buf(s.begin(), s.end());
        return DSink->Write(buf);
    }
};

CXMLWriter::CXMLWriter(std::shared_ptr<CDataSink> sink)
    : DImplementation(std::make_unique<SImplementation>(sink)) {}

CXMLWriter::~CXMLWriter() = default;

bool CXMLWriter::WriteEntity(const SXMLEntity &entity) {
    if (entity.DType == SXMLEntity::EType::StartElement) {
        std::string out = "<" + entity.DNameData;

        for (const auto &attr : entity.DAttributes) {
            out += " " + attr.first + "=\"" + EscapeAttr(attr.second) + "\"";
        }

        out += ">";
        if (!DImplementation->WriteString(out)) return false;

        DImplementation->DOpen.push_back(entity.DNameData);
        return true;
    }

    if (entity.DType == SXMLEntity::EType::EndElement) {
        std::string out = "</" + entity.DNameData + ">";
        if (!DImplementation->WriteString(out)) return false;

        // Pop if it matches the stack
        if (!DImplementation->DOpen.empty() && DImplementation->DOpen.back() == entity.DNameData) {
            DImplementation->DOpen.pop_back();
        }
        return true;
    }

    if (entity.DType == SXMLEntity::EType::CompleteElement) {
        std::string out = "<" + entity.DNameData;

        for (const auto &attr : entity.DAttributes) {
            out += " " + attr.first + "=\"" + EscapeAttr(attr.second) + "\"";
        }

        out += "/>";
        return DImplementation->WriteString(out);
    }

    if (entity.DType == SXMLEntity::EType::CharData) {
        // Escaped text
        return DImplementation->WriteString(EscapeText(entity.DNameData));
    }

    return false;
}

bool CXMLWriter::Flush() {
    // Close all still-open tags in reverse order
    while (!DImplementation->DOpen.empty()) {
        std::string name = DImplementation->DOpen.back();
        DImplementation->DOpen.pop_back();

        std::string out = "</" + name + ">";
        if (!DImplementation->WriteString(out)) return false;
    }
    return true;
}
