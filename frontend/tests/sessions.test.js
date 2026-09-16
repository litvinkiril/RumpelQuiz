import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, ApiError} from '../app.js';

function setup(results) {
  const calls = [];
  let saved = JSON.stringify({token: 'expired', refreshToken: 'old-refresh', sessionId: 's1'});
  const storage = {getItem: () => saved, setItem: (_, value) => {saved = value;}, removeItem: () => {saved = '{}';}};
  const controller = createController({storage, api: async (...args) => {
    calls.push(args);
    const next = results.shift();
    if (next instanceof Error) throw next;
    return next;
  }});
  return {controller, calls, saved: () => JSON.parse(saved)};
}
const expired = () => new ApiError('expired', 401);
const pair = {access_token: 'new-access', refresh_token: 'new-refresh', session_id: 's1'};

test('restored session rotates expired access and persists the new pair', async () => {
  const {controller: c, calls, saved} = setup([expired(), pair, {user_id: 'u1'}]);
  assert.equal(await c.start(), true);
  assert.deepEqual(calls, [['me', undefined, 'expired'], ['refresh', {session_id: 's1', refresh_token: 'old-refresh'}], ['me', undefined, 'new-access']]);
  assert.equal(saved().refreshToken, 'new-refresh');
  assert.equal(c.state.screen, 'account');
});
test('network failure after rotation preserves the usable refresh token', async () => {
  const {controller: c, saved} = setup([expired(), pair, new ApiError('offline')]);
  assert.equal(await c.start(), false);
  assert.equal(saved().refreshToken, 'new-refresh');
});
test('revoked refresh clears all credentials without retrying forever', async () => {
  const {controller: c, calls, saved} = setup([expired(), expired()]);
  assert.equal(await c.start(), false);
  assert.equal(calls.length, 2);
  assert.equal(saved().token, '');
  assert.equal(saved().refreshToken, '');
  assert.equal(saved().sessionId, '');
});
test('logout with expired access refreshes then revokes the session', async () => {
  const {controller: c, calls, saved} = setup([expired(), pair, {success: true}]);
  assert.equal(await c.logout(), true);
  assert.deepEqual(calls[2], ['logout', {}, 'new-access']);
  assert.equal(saved().refreshToken, '');
});
test('failed server logout keeps credentials so it can be retried', async () => {
  const {controller: c, saved} = setup([new ApiError('offline')]);
  assert.equal(await c.logout(), false);
  assert.equal(saved().refreshToken, 'old-refresh');
  assert.equal(c.state.success, '');
});
