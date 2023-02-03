/*
 * Copyright 2023 Marek Rogalski
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the standard MIT license. See LICENSE for more details.
 */

///////////////////////////////////////////////////////////////
// Easy to use JavaScript wrappers around the C++ functions. //
///////////////////////////////////////////////////////////////

// Helper function for computing the cost of a task.
//
// Usage:
//
//   let cost = Module.compute_cost({ travel_time: 10, work_time: 2, retry_risk: 0.2, priority: 3 });
//   console.log(cost); // 5
Module['compute_cost'] = function (obj) {
  return Module.C_ComputeCost(obj.travel_time || 0, obj.work_time || 0, obj.retry_risk || 0, obj.priority || 1);
};

// Main function for optimizing task assignments.
//
// The function takes a list of assignments (objects with `character`, `task` and `cost` fields)
// and returns optimized list of assignments - with at most one task per character.
// The `character` and `task` fields should be strings (or string-convertible).
// The `cost` field should be a number >= 0.
//
// Usage:
//
//   let assignments = [{ character: "John", task: "mop up blood at 10,10", cost: 10 },
//                      { character: "Fred", task: "mop up blood at 10,10", cost: 15 },
//                      { character: "John", task: "construct wall at 15,15", cost: 20 },
//                      { character: "Fred", task: "construct wall at 15,15", cost: 10 }];
//   let optimized = Module.optimize(assignments);
//   console.log(optimized); // [{ character: "John", task: "mop up blood at 10,10", cost: 10 },
//                           //  { character: "Fred", task: "construct wall at 15,15", cost: 10 }]
Module['optimize'] = function (assignments) {
  let assignments_c = new Module['C_vector$Assignment$']();
  // C++ code expects characters and tasks to be small integers.
  // We map strings to integers and back.
  let character_to_x = {};
  let task_to_y = {};
  for (let i = 0; i < assignments.length; i++) {
    let a = assignments[i];
    let x = a.character in character_to_x ? character_to_x[a.character] : character_to_x[a.character] = Object.keys(character_to_x).length;
    let y = a.task in task_to_y ? task_to_y[a.task] : task_to_y[a.task] = Object.keys(task_to_y).length;
    assignments_c.push_back({ character: x, task: y, cost: a.cost});
  }
  let x_to_character = Object.keys(character_to_x).reduce((acc, x) => { acc[character_to_x[x]] = x; return acc; }, Array());
  let y_to_task = Object.keys(task_to_y).reduce((acc, y) => { acc[task_to_y[y]] = y; return acc; }, Array());
  Module.C_Optimize(assignments_c);
  let optimized = [];
  let size = assignments_c.size();
  for (let i = 0; i < size; i++) {
    let result = assignments_c.get(i);
    optimized.push({ character: x_to_character[result.character],
                     task: y_to_task[result.task],
                     cost: result.cost });
  }
  assignments_c.delete();
  return optimized;
}
