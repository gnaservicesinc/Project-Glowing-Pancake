.pragma library

// Shared by the 3D headquarters and every 2D premises preview.
function normalized(kind) { return kind === "oak" || kind === "carpet" ? kind : "concrete" }
function source(kind) { return "qrc:/pgp/assets/ui/hq-" + normalized(kind) + "-floor.svg" }
function base(kind) { return kind === "oak" ? "#c4a47c" : kind === "carpet" ? "#78848a" : "#a6aba8" }
function roughness(kind) { return kind === "oak" ? 0.66 : kind === "carpet" ? 1.0 : 0.94 }
