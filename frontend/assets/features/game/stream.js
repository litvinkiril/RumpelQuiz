export async function consumeGameStream(response, onEvent) {
  if (!response.body?.getReader) throw new Error('Streaming response unavailable');
  const reader = response.body.getReader(),
    decoder = new TextDecoder();
  let buffer = '';
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buffer += decoder.decode(value, { stream: true });
      let end;
      while ((end = buffer.indexOf('\n\n')) !== -1) {
        const block = buffer.slice(0, end);
        buffer = buffer.slice(end + 2);
        let type = 'message';
        const data = [];
        for (const line of block.split('\n')) {
          if (line.startsWith('event:')) type = line.slice(6).trimStart();
          if (line.startsWith('data:')) data.push(line.slice(5).trimStart());
        }
        if (data.length && (await onEvent(type, JSON.parse(data.join('\n')))) === false) {
          await reader.cancel();
          return;
        }
      }
      if (buffer.length > 1048576) throw new Error('Game event too large');
    }
  } finally {
    reader.releaseLock();
  }
}
