const express = require('express');
const path = require('path');
const app = express();
const PORT = 3000;

// Create standard placeholder page
const htmlContent = `<!DOCTYPE html>
<html>
<head>
    <title>Serene Reader</title>
    <style>
        body {
            background: #111113;
            color: #C5C5CA;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            overflow: hidden;
        }
        .container {
            text-align: center;
            max-width: 500px;
            padding: 20px;
        }
        h1 {
            font-family: Georgia, serif;
            font-weight: normal;
            color: #E2E2E6;
            margin-bottom: 8px;
        }
        p {
            font-size: 14px;
            color: #8C8C92;
            line-height: 1.6;
        }
        .tag {
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 0.1em;
            text-transform: uppercase;
            color: #4A5D4E;
            margin-bottom: 24px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="tag">Windows Native Environment</div>
        <h1>Serene Reader Qt Core</h1>
        <p>This workspace is configured as a high-performance, lightweight native C++ & Qt project. To prevent web overhead and run with 0MB of web system memory, the web representation has been replaced with this placeholder. Read and write native files directly.</p>
    </div>
</body>
</html>`;

app.get('/', (req, res) => {
    res.send(htmlContent);
});

app.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running on port ${PORT}`);
});
