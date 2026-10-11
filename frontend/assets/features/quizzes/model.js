export const newAnswer = () => ({ text: '', is_correct: false, image_id: null, image_url: '' });
export const newQuestion = () => ({
  text: '',
  type: 'single',
  time_seconds: null,
  image_id: null,
  image_url: '',
  answers: [newAnswer(), newAnswer()],
});
export const newQuiz = (university) => ({
  university_id: university || '',
  name: '',
  description: '',
  default_time_seconds: 60,
  status: 'draft',
  questions: [newQuestion()],
});
const validTime = (n) => Number.isInteger(Number(n)) && Number(n) > 0 && Number(n) <= 2147483647;
export function questionComplete(q, defaultTime) {
  const count = q.answers.filter((a) => a.is_correct).length;
  return (
    !!q.text.trim() &&
    validTime(q.time_seconds ?? defaultTime) &&
    q.answers.length >= 2 &&
    q.answers.every((a) => a.text.trim() || a.image_id) &&
    (q.type === 'single' ? count === 1 : count >= 1)
  );
}
export function quizPayload(draft, status) {
  return {
    university_id: draft.university_id,
    name: draft.name,
    description: draft.description,
    default_time_seconds: Number(draft.default_time_seconds),
    status,
    ...(draft.id ? { revision: draft.revision } : {}),
    questions: draft.questions.map((q) => ({
      text: q.text,
      type: q.type,
      time_seconds: q.time_seconds === null ? null : Number(q.time_seconds),
      image_id: q.image_id,
      answers: q.answers.map((a) => ({
        text: a.text,
        is_correct: a.is_correct,
        image_id: a.image_id,
      })),
    })),
  };
}
