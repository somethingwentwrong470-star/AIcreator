# AIcreator (AI Assistant for Geometry Dash Editor)

A **Geode** mod (Geometry Dash 2.208+) that brings an AI building assistant directly into the Level Editor. Ask questions about level creation or let the AI place blocks, spikes, orbs, and portals on the grid from your text prompt.

## Features
* **In-Editor Chat Window** — open via the brain button in the top bar, drag by the header, and resize from the bottom-right corner.
* **Smart Object Placement** — ask the AI to build jumps, spike obstacles, or portals, and it spawns them aligned to the editor grid above the ground line.
* **Free Cloudflare Workers AI** — up to **10,000 neurons/day** for free (around 150–300 requests per day with no credit card required).
* **Emergency Stop (`STOP`)** — cancel any active AI request immediately.

## Installation
1. Download the latest `.geode` file from [**Releases**](../../releases).
2. Open **Geometry Dash** with **Geode** installed.
3. Go to the Geode mods menu -> **Install from File** (or place the file into `geode/mods`) and restart the game.

## Setup (Free Cloudflare Keys)
1. Sign up or log in at [Cloudflare Dashboard](https://cloudflare.com).
2. Open **AI** -> **Workers AI** and click **Use REST API**.
3. Copy your 32-character **Account ID** and create a **Workers AI API Token**.
4. Enter both values in one of two ways:
   * **In-Game Settings:** open **AIcreator** settings in Geode and paste your `Cloudflare Account ID` and `Cloudflare API Token`.
   * **Config File:** open the editor once to auto-generate `ai_config.txt` in `geode/config/custom.ai_editor_assistant/ai_config.txt` and fill it in:
     ```env
     CF_ID=your_32_char_account_id
     CF_KEY=your_api_token
     ```

## Example Prompts
* *"Build 3 spike jumps"*
* *"Place a ship portal with blocks above"*
* *"How do I set up a Move trigger?"*
* 
