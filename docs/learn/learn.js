'use strict';

const prediction = document.getElementById('prediction-result');
for (const button of document.querySelectorAll('[data-answer]')) {
  button.addEventListener('click', () => {
    for (const choice of document.querySelectorAll('[data-answer]')) {
      choice.setAttribute('aria-pressed', String(choice === button));
    }
    prediction.textContent = button.dataset.answer === 'drawing'
      ? 'Exactly. The overlay draws measurements; the attached module keeps sensing. Now verify it in Map Lab.'
      : 'Try separating the two switches: the overlay controls the drawing; “Sonar attached” controls the measurement. Verify the difference in Map Lab.';
  });
}

for (const button of document.querySelectorAll('[data-copy]')) {
  button.addEventListener('click', async () => {
    const source = document.getElementById(button.dataset.copy);
    const status = document.getElementById('copy-status');
    try {
      await navigator.clipboard.writeText(source.textContent.trim());
      status.textContent = button.dataset.copy === 'agent-prompt'
        ? 'Agent prompt copied. Paste it into your agent with the repository open.'
        : 'Commands copied. Run them in your terminal.';
      const label = button.textContent;
      button.textContent = 'Copied';
      setTimeout(() => { button.textContent = label; }, 2000);
    } catch {
      // Plain-text content stays usable without clipboard permission or HTTPS.
      const range = document.createRange();
      range.selectNodeContents(source);
      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      status.textContent = 'Clipboard unavailable. The text is selected; copy it manually.';
    }
  });
}
