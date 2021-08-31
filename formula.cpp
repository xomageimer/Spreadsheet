#include "formula.h"
#include "Engine.h"

#include "FormulaBaseListener.h"
#include "FormulaParser.h"

class BailErrorListener : public antlr4::BaseErrorListener {
public:
    void syntaxError(antlr4::Recognizer* /* recognizer */,
                     antlr4::Token* /* offendingSymbol */, size_t /* line */,
                     size_t /* charPositionInLine */, const std::string& msg,
                     std::exception_ptr /* e */
    ) override {
        throw std::runtime_error("Error when lexing: " + msg);
    }
};

std::unique_ptr<IFormula> ParseFormula(std::string expression) {
    return std::make_unique<FormulaCell>(expression);
}