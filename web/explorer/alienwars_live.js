/* Official Explorer's WASM transport, backed by the parent Map Lab's world. */
function alienwars_live() {
  return Promise.resolve({cwrap(name) {
    if (name !== 'flecs_explorer_request') throw new Error('Unknown Explorer export');
    return (method, path) => window.parent.awExplorerRequest(method, path);
  }});
}
