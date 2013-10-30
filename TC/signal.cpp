#include "signal.h"

#include "../lex/interface/lex_factory.h"
#include "tc_bar.h"

Signal::Signal(std::unique_ptr<lex::SignalRunnerInterface> runner) : runner_(std::move(runner))
{

}

Signal::~Signal()
{
}

Signal* Signal::GetSignal(std::string name, TCBarInterface *bar)
{
    lex::LexInterface *li = lex::LexFactory::CreateLexInterface();
    lex::SignalDetail sd = li->GetSignalDetail(name);
    auto runner = li->NewSignal(name, sd, bar);
    Signal *signal = new Signal(std::move(runner));
    return signal;
}
