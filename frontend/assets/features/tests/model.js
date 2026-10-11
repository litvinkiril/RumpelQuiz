import { newAnswer } from '../quizzes/model.js';
export function testSeconds(state, now = Date.now()) {
  const d = state.testPlay;
  if (d?.attempt.status !== 'in_progress') return 0;
  return Math.max(
    0,
    Math.ceil(
      (d.attempt.deadline_at_ms -
        d.server_now_ms -
        Math.max(0, now - (state.testReceivedAt ?? now))) /
        1000,
    ),
  );
}
export const newTestQuestion = () => ({
  text: '',
  type: 'single',
  image_id: null,
  image_url: '',
  answers: [newAnswer(), newAnswer()],
});
export const newTest = (university) => ({
  university_id: university || '',
  name: '',
  description: '',
  time_to_complete: 60,
  status: 'draft',
  questions: [newTestQuestion()],
});
export function testQuestionComplete(q) {
  const count = q.answers.filter((a) => a.is_correct).length;
  return (
    !!q.text.trim() &&
    q.answers.length >= 2 &&
    q.answers.every((a) => a.text.trim() || a.image_id) &&
    (q.type === 'single' ? count === 1 : count >= 1)
  );
}
export function testPayload(d, status) {
  return {
    university_id: d.university_id,
    name: d.name,
    description: d.description,
    time_to_complete: Number(d.time_to_complete),
    status,
    ...(d.id ? { revision: d.revision } : {}),
    questions: d.questions.map((q) => ({
      text: q.text,
      type: q.type,
      image_id: q.image_id,
      answers: q.answers.map((a) => ({
        text: a.text,
        is_correct: a.is_correct,
        image_id: a.image_id,
      })),
    })),
  };
}
