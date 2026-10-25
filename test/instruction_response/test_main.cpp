#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "game/InstructionResponse.h"
using Phase=InstructionResponse::Phase;
static uint32_t zero(uint32_t) { return 0; }
static uint8_t answer(const InstructionResponse &game) {
 for (uint8_t p=0;p<3;++p) if (game.itemAt(p)==game.targetItem()) return 1<<p;
 return 0;
}
static void begin(InstructionResponse &game,uint32_t at=0) {game.start({},zero);game.ready(at,0);}
static uint32_t draws[4], bounds[4];
static uint8_t drawIndex;
static uint32_t scripted(uint32_t bound) {
 assert(drawIndex<4 && bound==bounds[drawIndex] && draws[drawIndex]<bound);
 return draws[drawIndex++];
}
static void startDraw(InstructionResponse &game,const InstructionResponseSettings &settings,
                      uint32_t targetDraw,uint32_t shuffle3=0,uint32_t shuffle2=0) {
 bounds[0]=static_cast<uint32_t>(settings.targetWeights[0])+settings.targetWeights[1]+settings.targetWeights[2];
 bounds[1]=3;bounds[2]=2;draws[0]=targetDraw;draws[1]=shuffle3;draws[2]=shuffle2;drawIndex=0;
 game.start(settings,scripted);assert(drawIndex==3);
}
static void testWeights() {
 InstructionResponse game;
 InstructionResponseSettings settings;
 settings.targetWeights[0]=2;settings.targetWeights[1]=0;settings.targetWeights[2]=3;
 for(uint32_t draw=0;draw<5;++draw) {
  startDraw(game,settings,draw);assert(game.targetItem()==(draw<2?0:2));
 }
 for(uint8_t item=0;item<3;++item) settings.targetWeights[item]=65535;
 const uint32_t edges[]={0,65534,65535,131069,131070,196604};
 for(uint8_t edge=0;edge<6;++edge) {
  startDraw(game,settings,edges[edge]);assert(game.targetItem()==edge/2);
 }
 // Each of the six Fisher-Yates draws yields one distinct permutation even
 // when only item 2 can be a target. Target weights never become position weights.
 settings.targetWeights[0]=settings.targetWeights[1]=0;settings.targetWeights[2]=65535;
 bool permutations[27]={};unsigned seen=0;
 for(uint32_t a=0;a<3;++a) for(uint32_t b=0;b<2;++b) {
  startDraw(game,settings,65534,a,b);assert(game.targetItem()==2);
  const uint8_t key=game.itemAt(0)*9+game.itemAt(1)*3+game.itemAt(2);
  assert(!permutations[key]);permutations[key]=true;++seen;
 }
 assert(seen==6);
 settings={};
 for(uint32_t draw=0;draw<3;++draw) {startDraw(game,settings,draw);assert(game.targetItem()==draw);}
}
static void testConfiguredGame(uint8_t count,uint16_t answerMs,uint16_t feedbackMs) {
 InstructionResponse game;
 InstructionResponseSettings settings;
 settings.roundCount=count;settings.answerMs=answerMs;settings.feedbackMs=feedbackMs;
 settings.targetWeights[0]=0;settings.targetWeights[1]=65535;settings.targetWeights[2]=0;
 const auto configured=settings;
 game.start(settings,zero);
 // The active game's configuration is a snapshot, including its distribution.
 settings={};
 uint32_t now=0xfffff000u;
 for(uint8_t round=0;round<count;++round) {
  assert(game.targetItem()==1 && game.phase()==Phase::Preparing);
  game.ready(now,0);
  game.input(answer(game),answer(game),now+answerMs-1);
  game.update(now+answerMs-1);assert(game.phase()==Phase::Answer);
  game.update(now+answerMs);assert(game.phase()==Phase::Feedback && game.correct());
  game.update(now+answerMs+5000);assert(game.phase()==Phase::Feedback);
  // Feedback gets its whole configured duration after the first actual draw.
  now+=answerMs+5000;game.ready(now,0);
  game.update(now+feedbackMs-1);assert(game.phase()==Phase::Feedback);
  game.update(now+feedbackMs);now+=feedbackMs;
 }
 assert(game.phase()==Phase::Finished && game.completedRounds()==count);
 game.start(configured,zero);game.ready(0,0);
 game.input(answer(game),answer(game),configured.answerMs);game.update(configured.answerMs);
 assert(!game.correct());
}
static void testModes() {
 for(bool hard : {false,true}) for(bool noGo : {false,true}) for(unsigned scenario=0;scenario<7;++scenario) {
  InstructionResponseSettings settings;settings.hard=hard;settings.noGoEnabled=noGo;
  settings.typeWeights[0]=0;settings.typeWeights[1]=65535;
  InstructionResponse game;game.start(settings,zero);assert(game.isNoGo()==noGo);game.ready(0,0);
  const auto target=answer(game);const uint8_t wrong=target==1?2:1;
  auto press=[&](uint8_t mask,uint32_t at){game.input(mask,mask,at);game.input(0,0,at+1);};
  if(scenario==1) press(target,1);
  if(scenario==2){press(wrong,1);press(target,3);}
  if(scenario==3){press(target,1);press(wrong,3);}
  if(scenario==4) press(wrong,1);
  if(scenario==5) press(7,1);
  if(scenario==6) press(target,2000);
  game.update(1999);assert(game.phase()==Phase::Answer);
  game.update(2000);assert(game.phase()==Phase::Feedback);
  const bool expected=noGo ? (scenario==0 || scenario==6) : (scenario==1 || (!hard && (scenario==2 || scenario==3 || scenario==5)));
  assert(game.correct()==expected);
 }
 InstructionResponseSettings settings;settings.noGoEnabled=true;
 settings.typeWeights[0]=settings.typeWeights[1]=65535;
 for(uint32_t draw : {0u,65534u,65535u,131069u}) {
  bounds[0]=3;bounds[1]=131070;bounds[2]=3;bounds[3]=2;
  draws[0]=0;draws[1]=draw;draws[2]=0;draws[3]=0;drawIndex=0;
  InstructionResponse game;game.start(settings,scripted);
  assert(game.isNoGo()==(draw>=65535) && drawIndex==4);
 }
 puts("[PASS] easy/hard x normal/no-go whole-window input matrix; exclusive deadline; independent max-weight type boundaries");
}
int main() {
 testModes();
 testWeights();testConfiguredGame(1,500,100);testConfiguredGame(20,10000,10000);
 InstructionResponse game;
 game.start({},zero);game.update(5000);assert(game.phase()==Phase::Preparing);
 game.ready(6000,2);game.update(9000);assert(game.phase()==Phase::Preparing);
 game.input(0,0,9000);assert(game.phase()==Phase::Answer);
 const uint8_t target=answer(game),wrong=target==1?2:1;
 game.input(wrong,wrong,9001);game.input(0,0,9002);game.input(target,target,10999);
 game.update(10999);assert(game.phase()==Phase::Answer);
 game.update(11000);assert(game.phase()==Phase::Feedback && game.correct());
 game.ready(11000, 0);
 game.update(11499);assert(game.phase()==Phase::Feedback);
 game.update(11500);assert(game.phase()==Phase::Preparing);
 game.ready(11500,target);game.update(15000);assert(game.phase()==Phase::Preparing);
 game.input(0,0,15000);game.update(17000);assert(!game.correct());
 begin(game);game.input(answer(game),answer(game),2000);game.update(2000);assert(!game.correct());
 begin(game);game.input(answer(game),answer(game),1999);game.update(2100);assert(game.correct());
 begin(game);game.input(5,5,100);game.input(0,0,1100);assert(game.phase()==Phase::Cancelled);
 begin(game);game.input(7,7,1);game.update(999);assert(game.phase()==Phase::Answer);game.input(0,0,999);game.update(2000);assert(game.correct());
 begin(game);game.input(5,5,1500);game.update(2000);assert(game.phase()==Phase::Answer);game.update(2500);assert(game.phase()==Phase::Cancelled);
 begin(game);game.input(5,5,1990);game.update(2000);game.input(0,0,2200);game.update(2200);assert(game.phase()==Phase::Feedback);
 const uint32_t start=0xfffffc00u;begin(game,start);game.input(answer(game),answer(game),start+1999);game.update(start+2000);assert(game.correct());
 begin(game);uint32_t now=0;
 for (unsigned round=0;round<10;++round) {
  game.ready(now,0);game.input(answer(game),answer(game),now+1);game.update(now+1999);assert(game.phase()==Phase::Answer);
  game.update(now+2000);assert(game.correct());game.ready(now+2000,0);game.update(now+2500);now+=2500;
 }
 assert(game.phase()==Phase::Finished && game.completedRounds()==10);
 puts("[PASS] configured 1/20 rounds, min/max times, frozen settings, 32-bit weight boundaries, zero exclusion, repeated targets, six independent shuffles, delayed feedback readiness, release/input/cancel/rollover/default completion");
}
