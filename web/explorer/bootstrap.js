/* A standalone visit opens the simulation first; there is no second ECS world. */
if (window.parent === window || !window.parent.awExplorerRequest) {
  location.replace('../maplab/?explorer=1');
}
