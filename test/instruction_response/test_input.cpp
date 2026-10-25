#include <assert.h>
#include "platform/ButtonInput.h"
#include "game/InstructionResponse.h"
#include <stdio.h>
static unsigned long now=0;
static int pins[3]={HIGH,HIGH,HIGH};
static int callbacks=0;
unsigned long millis() { return now; }
int digitalRead(int pin) {return pins[pin];}
void pinMode(int,int) {}
int digitalPinToInterrupt(int pin) {return pin;}
void attachInterrupt(int,void (*)(),int) {}
void callback() {++callbacks;}
void sampleGame(ButtonInput &buttons, InstructionResponse &game)
{
    for(uint8_t edge=0;edge<3;++edge) {
        uint8_t held,pressed;uint32_t at;
        buttons.instructionSample(held,pressed,at);
        game.input(held,pressed,at);
    }
    game.pendingInput(buttons.instructionPending(),buttons.instructionPendingAt(),buttons.instructionRawMask());
    game.update(static_cast<uint32_t>(now));
}
void testStaggeredDeadline(uint32_t base,bool hard,bool noGo,unsigned duration)
{
    pins[0]=pins[1]=pins[2]=HIGH;now=base;
    ButtonInput buttons(0,1,2);buttons.clearFlags();
    InstructionResponseSettings settings;settings.answerMs=500;settings.hard=hard;settings.noGoEnabled=noGo;
    settings.typeWeights[0]=0;settings.typeWeights[1]=1;
    InstructionResponse game;game.start(settings,[](uint32_t){return 0u;});
    sampleGame(buttons,game);now=base+20u;sampleGame(buttons,game);game.ready(static_cast<uint32_t>(now),0);
    now=base+510u;pins[1]=LOW;sampleGame(buttons,game);
    now=base+515u;pins[0]=LOW;sampleGame(buttons,game);
    now=base+520u;sampleGame(buttons,game);
    assert(game.phase()==InstructionResponse::Phase::Answer);
    now=base+530u;sampleGame(buttons,game);
    now=base+535u;sampleGame(buttons,game);
    assert(game.phase()==InstructionResponse::Phase::Answer);
    now=static_cast<uint32_t>(base+515u+duration);pins[1]=HIGH;sampleGame(buttons,game);
    if(duration>=1000) assert(game.phase()==InstructionResponse::Phase::Cancelled);
    else {
        assert(game.phase()==InstructionResponse::Phase::Answer);
        now=static_cast<uint32_t>(base+1515u);sampleGame(buttons,game);
        assert(game.phase()==InstructionResponse::Phase::Answer);
        now=static_cast<uint32_t>(base+1534u);sampleGame(buttons,game);
        assert(game.phase()==InstructionResponse::Phase::Feedback && game.correct()==(!hard&&!noGo));
    }
}
void testLateSecondKey()
{
    pins[0]=pins[1]=pins[2]=HIGH;now=0;
    ButtonInput buttons(0,1,2);buttons.clearFlags();
    InstructionResponse game;game.start({},[](uint32_t){return 0u;});
    sampleGame(buttons,game);now=20;sampleGame(buttons,game);game.ready(20,0);
    now=2010;pins[1]=LOW;sampleGame(buttons,game);
    now=2020;sampleGame(buttons,game);assert(game.phase()==InstructionResponse::Phase::Answer);
    now=2025;pins[0]=LOW;sampleGame(buttons,game);
    now=2030;sampleGame(buttons,game);
    assert(game.phase()==InstructionResponse::Phase::Feedback && !game.correct());
    now=2045;sampleGame(buttons,game);
    assert(!game.correct()); // The late target keeps its own timestamp, not the first key's.
}
int main()
{
    ButtonInput buttons(0,1,2);
    buttons.begin();buttons.clearFlags();
    uint8_t held=0,pressed=0;uint32_t timestamp=0;
    buttons.instructionSample(held,pressed,timestamp);assert(held==7 && pressed==0);
    now=20;for(unsigned edge=0;edge<3;++edge) buttons.instructionSample(held,pressed,timestamp);assert(held==0);
    pins[1]=LOW;now=30;buttons.instructionSample(held,pressed,timestamp);assert(pressed==0);
    now=49;buttons.instructionSample(held,pressed,timestamp);assert(pressed==0);
    now=50;buttons.instructionSample(held,pressed,timestamp);assert(held==1 && pressed==1 && timestamp==30);
    now=1000;buttons.instructionSample(held,pressed,timestamp);assert(held==1 && pressed==0);
    pins[0]=LOW;now=1010;buttons.instructionSample(held,pressed,timestamp);
    now=1030;buttons.instructionSample(held,pressed,timestamp);assert(held==5 && pressed==4 && timestamp==1010);
    // Return consumes still-held game keys before normal menu callbacks resume.
    buttons.clearFlags();
    buttons.update(callback,callback,callback,callback);assert(callbacks==0);
    pins[0]=pins[1]=HIGH;now=1040;buttons.update(callback,callback,callback,callback);assert(callbacks==0);
    pins[2]=LOW;now=2000;buttons.update(callback,callback,callback,callback);assert(callbacks==2);
    for(unsigned mode=0;mode<4;++mode) for(unsigned duration=999;duration<=1001;++duration) {
        testStaggeredDeadline(0,mode&1,mode&2,duration);
        testStaggeredDeadline(0xfffffc00u,mode&1,mode&2,duration);
    }
    testLateSecondKey();
    puts("[PASS] public per-key debounce preserves staggered predeadline timestamps and short/exact/long exits through rollover; held keys consumed on return");
}
