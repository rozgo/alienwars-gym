'use strict';

const prediction = document.getElementById('prediction-result');
for (const button of document.querySelectorAll('[data-answer]')) {
  button.addEventListener('click', () => {
    for (const choice of document.querySelectorAll('[data-answer]')) {
      choice.setAttribute('aria-pressed', String(choice === button));
    }
    prediction.textContent = button.dataset.answer === 'drawing'
      ? 'The drawing disappears, while the attached sensor keeps sampling. You can check this by watching the Sonar reading with the overlay hidden.'
      : 'Hiding the overlay leaves the sensor attached, so measurements continue. The “Sonar attached” control disables the module. Compare the readout after using each switch.';
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
