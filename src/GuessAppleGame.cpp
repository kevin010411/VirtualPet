#include "GuessAppleGame.h"

GuessAppleGame::GuessAppleGame(Pet *pet, Renderer *renderer, std::queue<Animation> *animQueue, bool *calloutAnimate)
    : pet(pet), renderer(renderer), animationQueue(animQueue), dirtyAnimate(calloutAnimate)
{
    reset();
}

void GuessAppleGame::start()
{
    reset();
    state = State::WaitingApple;
    lastMoveTime = millis();
}

bool GuessAppleGame::isActive() const { return state != State::Inactive; }

void GuessAppleGame::update(unsigned long dt)
{
    if (state == State::Inactive)
        return;

    unsigned long now = millis();
    String animate_idx = "";
    switch (state)
    {
    case State::WaitingApple:
        if (now - lastMoveTime > 700)
        {
            appleSide = (random(2) == 0) ? Side::Left : Side::Right;
            state = State::WaitingInput;
        }
        if (animationQueue->empty())
        {
            animate_idx = String(random(1, 4));
            animationQueue->push(Animation("apple_" + animate_idx, 20000, true));
            *dirtyAnimate = true;
        }
        break;

    case State::ShowingResult:
        if (animationQueue->empty())
        {
            if (correctCount >= 3)
            {
                state = State::Win;
                animationQueue->push(Animation("guess_win", 2000, true));
                *dirtyAnimate = true;
            }
            else if (wrongCount >= 3)
            {
                state = State::Lose;
                animationQueue->push(Animation("guess_loss", 2000, true));
                *dirtyAnimate = true;
            }
            else
            {
                state = State::WaitingApple;
                animationQueue->push(Animation("apple", 2000, true));
                *dirtyAnimate = true;
            }
        }
        break;

    case State::Win:
    case State::Lose:
        if (animationQueue->empty())
        {
            state = State::Inactive;
        }
        break;
    case State::WaitingInput:
        if (animationQueue->empty())
        {
            animate_idx = String(random(1, 4));
            animationQueue->push(Animation("apple_" + animate_idx, 20000, true));
            *dirtyAnimate = true;
        }
        break;

    default:
        break;
    }
}

void GuessAppleGame::onLeft()
{
    if (state == State::WaitingInput)
        handleGuess(Side::Left);
}

void GuessAppleGame::onRight()
{
    if (state == State::WaitingInput)
        handleGuess(Side::Right);
}

void GuessAppleGame::reset()
{
    correctCount = 0;
    wrongCount = 0;
    state = State::Inactive;
}

void GuessAppleGame::handleGuess(Side player)
{
    bool correct = (player == appleSide);
    String result = String(int(appleSide) + int(player) * 2 + 1);
    animationQueue->push(Animation("apple_p" + result, 2000, true));
    *dirtyAnimate = true;
    if (correct)
    {
        pet->changeMood(20);
        correctCount++;
        animationQueue->push(Animation("guess_right", 2000, true));
        *dirtyAnimate = true;
    }
    else
    {
        pet->changeMood(-10);
        wrongCount++;
        animationQueue->push(Animation("guess_wrong", 2000, true));
        *dirtyAnimate = true;
    }

    state = State::ShowingResult;
}