/*
 * Copyright 2023 Marek Rogalski
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the standard MIT license. See LICENSE for more details.
 */

///////////////////////////////////////////////////////////////
// Main header file for including LibColony in C++ projects. //
///////////////////////////////////////////////////////////////

/*

LibColony is an AI library for games like Dwarf Fortress. It's meant
to make the pawns smart enough so that manual micromanagement of jobs
is no longer necessary.

# Design

The main function of LibColony is assigning characters to tasks. Tasks
should be understood as anything that the character might do,
including eating or taking breaks. Tasks are assigned to the character
that is the "best fit" for a given task, while also taking into
account what other characters are "best fit" to do. The assignment is
performed with an algorithm that produces optimal assignments.
The following factors are taken into account in the process:

 - travel time to the task location
 - time necessary to execute the task
 - risk of a failure and potential retry
 - priority of the task

Each character can view the tasks differently. This means that the factors
listed above can be different for each pawn.

The "work" and "leisure" tasks are treated uniformly by the algorithm. This
leads to several emergent behaviors. Usually characters take lunch breaks during
work if they get hungry but will keep working a bit longer if the task at hand
is close to completion. Distance to the food also matters: proximity to snacks
encourages more frequent breaks. When one character is on the lunch break,
another nearby character can take over their task.

# Using LibColony

LibColony was designed to be part of games with highly dynamic
worlds. The pipeline can be re-computed on every animation
frame. Characters and tasks can be added and removed. Travel costs,
execution times, failure probabilities and perceived values of tasks
can change to reflect the game world.

## Including in C++ projects

The library is written inline in a single header file. Ideally this header
should be included in a single compilation unit and then linked into the final
executable.

## Complexity

Task assignment relies on the Hungarian algorithm which is O(n^3).
LibColony offers an optional optimization where restricting
the assignment to only top-n tasks reduces this to O(n^2). Assignment algorithm
is pretty optimized so even the O(n^3) variant should work fine for games with
hundreds of units & tasks. Only when the number of units goes into thousands,
the optimization becomes useful.

## Stability of assignments

In some scenarios the assignment may be flapping between two equally
good tasks. In order to stick to one of them, the costs (travel or
execution) of the assigned task should be decreasing a bit on every
frame.

## Hauling & delivery

In order to implement hauling, the pipeline should be started twice
per frame. The first execution should assign resources to delivery
destinations. This assignment creates the "delivery tasks" for the
second execution where characters are assigned to the tasks.

## Long-term planning

Sometimes a single pawn can execute many tasks alone faster than many (slower)
pawns would. This may actually happen quite often. For example when a few small
tasks pop up in a remote location, a nearby pawn can execute them all faster
than a bunch of pawns running from the other side of the map would.

LibColony can be used to find such solutions.

In order to do so first you have to run the task assignment as usual. The
initial assignment would assign the tasks to all available pawns - slow & fast.
Pick the task that's going to be completed first and mark it as completed. Move
the pawn that completed the task to its location & adjust its costs of
performing other tasks by increasing them by the cost of the finished task.
Afterwards run the task assignment again. The second repetition might reassign
one of the tasks from the slow pawns to the fast one. Repeat as many times as
you want (within your time bugdet or until all tasks are completed) recording
which tasks have been completed & tweaking the costs of other tasks done by
these pawns. The final long-term plan consits of the tasks which were finished
during this process (assigned to the pawns which completed them) and then to the
tasks which they were assigned in the final assignment.

## Personal tasks

Tasks in LibColony can be executed by one character at a time. To
support tasks which can be done by many characters, create an instance
of that task for each character.

*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace colony {

// This is a helper function that computes the cost of a task taking into
// account travel time, work time, risk of retry & priority.
//
// `travel_time` should be the time it takes to get to the task location (>=0)
// `work_time` should be the time it takes to execute the task (>=0)
// `retry_risk` should be the probability of failure [0,1)
// `priority` should be the priority of the task (>0)
//
// Note that computing cost of impossible tasks (with retry risk of 1 or
// priority of 0) doesn't make sense and will return infinity.
inline double ComputeCost(double travel_time = 0, double work_time = 0,
                          double retry_risk = 0, double priority = 1) {
  if (retry_risk >= 1 || priority <= 0) {
    return std::numeric_limits<double>::infinity();
  }
  double cost = travel_time + work_time;
  cost /= 1.0 - retry_risk;
  cost /= priority;
  return cost;
}

typedef int CharacterId;
typedef int TaskId;

struct Assignment {
  CharacterId character;
  TaskId task;
  double cost;
};

// Reduce the number of potential assignments for each character & task.
inline void LimitAssignments(std::vector<Assignment> &assignments,
                             int limit_per_character, int limit_per_task) {
  sort(
      assignments.begin(), assignments.end(),
      [](const Assignment &a, const Assignment &b) { return a.cost < b.cost; });
  CharacterId last_character = -1;
  TaskId last_task = -1;
  for (auto &a : assignments) {
    last_character = std::max(last_character, a.character);
    last_task = std::max(last_task, a.task);
  }
  int tasks_per_character[last_character + 1];
  int characters_per_task[last_task + 1];
  for (int i = 0; i <= last_character; ++i) {
    tasks_per_character[i] = 0;
  }
  for (int i = 0; i <= last_task; ++i) {
    characters_per_task[i] = 0;
  }

  for (int i = 0; i < assignments.size(); ++i) {
    auto &a = assignments[i];
    if (tasks_per_character[a.character] >= limit_per_character) {
      std::swap(a, assignments.back());
      assignments.pop_back();
      --i;
      continue;
    }
    if (characters_per_task[a.task] >= limit_per_task) {
      std::swap(a, assignments.back());
      assignments.pop_back();
      --i;
      continue;
    }
    ++tasks_per_character[a.character];
    ++characters_per_task[a.task];
  }
}

// The main function of this library. It takes a vector of potential assigments
// of characters to tasks and removes all assignments that are not optimal.
inline void Optimize(std::vector<Assignment> &assignments) {

  // Convert the problem into max-value assignment.
  CharacterId max_character = 0;
  TaskId max_task = 0;
  double max_cost = 0;
  for (auto &a : assignments) {
    max_character = std::max(max_character, a.character);
    max_task = std::max(max_task, a.task);
    max_cost = std::max(max_cost, a.cost);
  }

  int NX, NY;

  // The algorithm finds the optimal assignment only when NX <= NY.
  if (max_task > max_character) {
    NX = max_character + 1;
    NY = max_task + 1;
  } else {
    NX = max_task + 1;
    NY = max_character + 1;
  }

  double value[NX][NY];
  for (int x = 0; x < NX; ++x) {
    for (int y = 0; y < NY; ++y) {
      value[x][y] = 1.0;
    }
  }

  if (max_task > max_character) {
    for (auto &a : assignments) {
      value[a.character][a.task] = max_cost - a.cost + 1.0;
    }
  } else {
    for (auto &a : assignments) {
      value[a.task][a.character] = max_cost - a.cost + 1.0;
    }
  }

  int max_match;         // n workers and n jobs
  double lx[NX], ly[NY]; // labels of X and Y parts
  int xy[NX];            // xy[x] - vertex that is matched with x,
  int yx[NY];            // yx[y] - vertex that is matched with y
  bool S[NX], T[NY];     // sets S and T in algorithm
  double slack[NY];      // as in the algorithm description
  int slackx[NY];        // slackx[y] such a vertex, that
  // l(slackx[y]) + l(y) - w(slackx[y],y) = slack[y]
  int prev[NX]; // array for memorizing alternating paths

  auto update_labels = [&]() {
    int x, y;
    double delta = std::numeric_limits<double>::max();
    for (y = 0; y < NY; y++) // calculate delta using slack
      if (!T[y])
        delta = std::min(delta, slack[y]);
    for (x = 0; x < NX; x++) // update X labels
      if (S[x])
        lx[x] -= delta;
    for (y = 0; y < NY; y++) // update Y labels
      if (T[y])
        ly[y] += delta;
    for (y = 0; y < NY; y++) // update slack array
      if (!T[y])
        slack[y] -= delta;
  };

  // x - current vertex,prevx - vertex from X before x in the alternating
  // path, so we add edges (prevx, xy[x]), (xy[x], x)
  auto add_to_tree = [&](int x, int prevx) {
    S[x] = true;     // add x to S
    prev[x] = prevx; // we need this when augmenting
    for (int y = 0; y < NY;
         y++) // update slacks, because we add new vertex to S
      if (lx[x] + ly[y] - value[x][y] < slack[y]) {
        slack[y] = lx[x] + ly[y] - value[x][y];
        slackx[y] = x;
      }
  };

  max_match = 0; // number of vertices in current matching
  memset(xy, -1, sizeof(xy));
  memset(yx, -1, sizeof(yx));

  memset(lx, 0.0, sizeof(lx));
  memset(ly, 0.0, sizeof(ly));
  for (int x = 0; x < NX; x++)
    for (int y = 0; y < NY; y++)
      lx[x] = std::max(lx[x], value[x][y]);

  auto eq = [](double a, double b) { return std::abs(a - b) < 0.0001; };

  while (max_match < std::min(NX, NY)) {
    int x, y, root; // just counters and root vertex
    int q[NX], wr = 0,
               rd = 0; // q - queue for bfs, wr,rd - write and read pos in queue
    memset(S, false, sizeof(S)); // init set S
    memset(T, false, sizeof(T)); // init set T
    memset(prev, -1,
           sizeof(prev)); // init set prev - for the alternating tree
    double best_lx = std::numeric_limits<double>::min();
    for (x = 0; x < NX; x++) // finding root of the tree
      if (xy[x] == -1) {
        if (lx[x] > best_lx) {
          best_lx = lx[x];
          root = x;
        }
      }

    q[wr++] = root;
    prev[root] = -2;
    S[root] = true;

    for (y = 0; y < NY; y++) { // initializing slack array
      slack[y] = lx[root] + ly[y] - value[root][y];
      slackx[y] = root;
    }

    while (true) {               // main cycle
      while (rd < wr) {          // building tree with bfs cycle
        x = q[rd++];             // current vertex from X part
        for (y = 0; y < NY; y++) // iterate through all edges in equality
                                 // graph
          if (eq(value[x][y], lx[x] + ly[y]) && !T[y]) {
            if (yx[y] == -1)
              break;         // an exposed vertex in Y found, so augmenting path
                             // exists!
            T[y] = true;     // else just add y to T,
            q[wr++] = yx[y]; // add vertex yx[y], which is matched with y, to
                             // the queue
            add_to_tree(yx[y],
                        x); // add edges (x,y) and (y,yx[y]) to the tree
          }
        if (y < NY)
          break; // augmenting path found!
      }
      if (y < NY)
        break; // augmenting path found!

      update_labels(); // augmenting path not found, so improve labeling
      wr = rd = 0;
      for (y = 0; y < NY; y++)
        // in this cycle we add edges that were added to the equality graph as
        // a result of improving the labeling, we add edge (slackx[y], y) to
        // the tree if and only if !T[y] && slack[y] == 0, also with this edge
        // we add another one (y, yx[y]) or augment the matching, if y was
        // exposed
        if (!T[y] && eq(slack[y], 0)) {
          if (yx[y] ==
              -1) { // exposed vertex in Y found - augmenting path exists!
            x = slackx[y];
            break;
          } else {
            T[y] = true; // else just add y to T,
            if (!S[yx[y]]) {
              q[wr++] = yx[y]; // add vertex yx[y], which is matched with
              // y, to the queue
              add_to_tree(yx[y], slackx[y]); // and add edges (x,y) and (y,
              // yx[y]) to the tree
            }
          }
        }
      if (y < NY)
        break; // augmenting path found!
    }

    if (y < NY) {  // we found augmenting path!
      max_match++; // increment matching
      // in this cycle we inverse edges along augmenting path
      for (int cx = x, cy = y, ty; cx != -2; cx = prev[cx], cy = ty) {
        ty = xy[cx];
        yx[cy] = cx;
        xy[cx] = cy;
      }
    }
  }

  if (max_task > max_character) {
    for (int i = 0; i < assignments.size(); ++i) {
      if (assignments[i].task != xy[assignments[i].character]) {
        std::swap(assignments[i], assignments.back());
        assignments.pop_back();
        --i;
      }
    }
  } else {
    for (int i = 0; i < assignments.size(); ++i) {
      if (assignments[i].task != yx[assignments[i].character]) {
        std::swap(assignments[i], assignments.back());
        assignments.pop_back();
        --i;
      }
    }
    
  }
}

} // namespace colony
