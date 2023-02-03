#include "colony.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdio.h>
#include <vector>

using namespace std;

int SIZE = 256;

struct Character {
  int x;
  int y;
};

struct Work {
  int x;
  int y;
  int t;
};

bool game_over;
int stage;
const char *stage_name;
int stage_limit;
int steps;
vector<function<pair<const char *, int>()>> stages;
vector<Character> characters;
vector<Work> work;

pair<const char *, int> stage_random() {
  for (int i = 0; i < 50; ++i) {
    characters.push_back(Character{rand() % SIZE, rand() % SIZE});
  }
  for (int i = 0; i < 500; ++i) {
    work.push_back(Work{rand() % SIZE, rand() % SIZE, 10});
  }
  return make_pair("Random", 280);
}

pair<const char *, int> stage_row() {
  for (int i = 0; i < 50; ++i) {
    characters.push_back(Character{25, 25 + i * 4});
  }
  for (int i = 0; i < 50; ++i) {
    work.push_back(Work{225, 25 + i * 4, 10});
  }
  return make_pair("Row", 210);
}

pair<const char *, int> stage_skew() {
  for (int i = 0; i < 50; ++i) {
    characters.push_back(Character{25, 5 + i * 4});
  }
  for (int i = 0; i < 50; ++i) {
    work.push_back(Work{225, 50 + i * 4, 10});
  }
  return make_pair("Skewed row", 210);
}

pair<const char *, int> stage_thatguy() {
  for (int i = 0; i < 10; ++i) {
    characters.push_back(Character{25, 128 + (i - 5) * 4});
  }
  characters.push_back(Character{200, 128});
  for (int i = 0; i < 10; ++i) {
    work.push_back(Work{225, 128 + (i - 5) * 4, 10});
  }
  return make_pair("That guy", 161);
}

pair<const char *, int> stage_circle() {
  for (int i = 0; i < 50; ++i) {
    int x = (int)(128 + 120 * cos(i / 50. * M_PI * 2));
    int y = (int)(128 + 120 * sin(i / 50. * M_PI * 2));
    characters.push_back(Character{x, y});
  }
  work.push_back(Work{128, 128, 10});
  return make_pair("Circle", 97);
}

pair<const char *, int> stage_square() {
  for (int i = 0; i < 10; ++i) {
    characters.push_back(Character{128 + (i - 5) * 20, 128 + 100});
    characters.push_back(Character{128 + (i - 5) * 20, 128 - 100});
    characters.push_back(Character{128 + 100, 128 + (i - 5) * 20});
    characters.push_back(Character{128 - 100, 128 + (i - 5) * 20});
  }
  work.push_back(Work{128, 128, 10});
  return make_pair("Square", 110);
}

pair<const char *, int> stage_square2() {
  for (int i = 0; i < 10; ++i) {
    work.push_back(Work{128 + (i - 5) * 20, 128 + 100, 10});
    work.push_back(Work{128 + (i - 5) * 20, 128 - 100, 10});
    work.push_back(Work{128 + 100, 128 + (i - 5) * 20, 10});
    work.push_back(Work{128 - 100, 128 + (i - 5) * 20, 10});
  }
  for (int i = 0; i < 40; ++i) {
    characters.push_back(Character{128, 128});
  }
  return make_pair("Inversed square", 110);
}

pair<const char *, int> stage_insane() {
  for (int i = 0; i < 500; ++i) {
    characters.push_back(Character{rand() % SIZE, rand() % SIZE});
  }
  for (int i = 0; i < 2000; ++i) {
    work.push_back(Work{rand() % SIZE, rand() % SIZE, 10});
  }
  return make_pair("Insane", 101); // Usually will be ~70
}
void init() {
  game_over = false;
  stage = -1;
  stages.push_back(stage_random);
  stages.push_back(stage_square);
  stages.push_back(stage_square2);
  stages.push_back(stage_circle);
  stages.push_back(stage_thatguy);
  stages.push_back(stage_row);
  stages.push_back(stage_skew);
  stages.push_back(stage_insane);
}

int sign(int value) {
  if (value == 0) {
    return 0;
  } else if (value > 0) {
    return 1;
  } else {
    return -1;
  }
}

int abs(int value) {
  if (value < 0)
    return -value;
  return value;
}

void step() {
  for (int i = 0; i < work.size(); ++i) {
    if (work[i].t == 0) {
      swap(work[i], work[work.size() - 1]);
      work.pop_back();
      --i;
    }
  }

  ++steps;
  if (work.size() == 0) {
    if (stage >= 0) {
      printf("Stage '%s' completed in %d / %d steps\n", stage_name, steps,
             stage_limit);
      steps = 0;
    }
    ++stage;
    if (stage >= stages.size()) {
      game_over = true;
      return;
    }
    characters.clear();
    work.clear();
    auto ret = stages[stage]();
    stage_name = ret.first;
    stage_limit = ret.second;
  }

  vector<colony::Assignment> assignments;
  for (int i = 0; i < characters.size(); ++i) {
    for (int j = 0; j < work.size(); ++j) {
      double cost = (double)max(abs(characters[i].x - work[j].x),
                                abs(characters[i].y - work[j].y)) +
                    work[j].t;
      assignments.push_back(colony::Assignment{i, j, cost});
    }
  }

  auto start = std::chrono::steady_clock::now();
  colony::Optimize(assignments);
  auto end = std::chrono::steady_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
  printf("Optimization took %lf ms\n", us / 1000.);

  for (int i = 0; i < assignments.size(); ++i) {
    auto &a = assignments[i];
    auto &c = characters[a.character];
    auto &w = work[a.task];
    int dx = w.x - c.x;
    int dy = w.y - c.y;
    c.x += sign(dx);
    c.y += sign(dy);
    if (dx == 0 && dy == 0) {
      --w.t;
    }
  }
  for (auto &c : characters) {
    c.x %= SIZE;
    c.y %= SIZE;
  }
}

//////////////
// SDL part //
//////////////

SDL_Window *window;
SDL_Renderer *renderer;

int SCALE = 4;

void loop(void *arg) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

  for (auto &w : work) {
    SDL_Rect rect;
    rect.x = w.x * SCALE;
    rect.y = w.y * SCALE;
    rect.w = SCALE;
    rect.h = SCALE;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);
  }

  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
  for (auto &c : characters) {
    SDL_Rect rect;
    rect.x = c.x * SCALE;
    rect.y = c.y * SCALE;
    rect.w = SCALE;
    rect.h = SCALE;
    SDL_RenderDrawRect(renderer, &rect);
  }
  /*
    for (auto& a : colony.assignments) {
      auto& c = characters[a.character];
      auto& t = work[a.task];
      SDL_RenderDrawLine(renderer, c.x * SCALE, c.y * SCALE, t.x * SCALE, t.y *
    SCALE);
    }*/

  SDL_RenderPresent(renderer);
  if (game_over) {
    SDL_Quit();
  }

  step();

  // SDL_Delay(16);
}

extern "C" int main(int argc, char **argv) {
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_CreateWindowAndRenderer(SIZE * SCALE, SIZE * SCALE, 0, &window,
                              &renderer);
  init();
  while (!game_over)
    loop(nullptr);
  return 0;
}
