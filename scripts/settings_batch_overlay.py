"""Player extras, locale, hide-content, thumbnail and account screens are real
levels; the text prompt names the folder it opens and why a text was refused."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def screens():
    """Settings ids that open a list: the provider's own, then one per player-extras group."""
    groups = re.findall(r'\{ "(settings:[a-z-]+)", "', (ROOT / "native/tube_player_extra.c").read_text(encoding="utf-8"))
    if len(groups) != 49 or len(set(groups)) != 49:
        raise RuntimeError("Player-extras groups changed: update the settings screens")
    return ["settings:extra:captions", "settings:extra:audio", "settings:extra:player", "settings:extra:buttons",
            "settings:extra:dimming", "settings:content-language",
            "settings:content-country", "settings:hide", "settings:thumbnails", "settings:accounts",
            "settings:channel-sort", "settings:experimental", "settings:about", "settings:about:credits", "settings:about:changelog",
            "settings:backup", "settings:backup:list", "settings:backup:import"] + groups


def apply_settings_batch_overlay(replace, player, host):
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(ui, 'id != "settings:languages" && ',
            'id != "settings:languages" && ' + "".join(f'id != "{screen}" && ' for screen in screens())
            # One backup of the list is a level of its own: Circle returns to the list.
            + 'id.rfind("settings:backup:entry:", 0) != 0 && ')
    # One prompt serves playlist names, pin names, channel searches and profile passwords.
    replace(host, 'evo_rmlui_provider_replace_folder(parent, std::strncmp(parent, "settings:pin:", 13) ? "Confirm playlist change" : "Pinned section");',
            'evo_rmlui_provider_replace_folder(parent, tube_prompt_title(parent));')
    replace(host, 'else toast("PLAYLIST", "Enter a name of 1 to 150 characters for the current profile");',
            'else if (*tube_prompt_error()) toast("PROSPEROTUBE", tube_prompt_error());\n'
            '                    else toast("PLAYLIST", "Enter a name of 1 to 150 characters for the current profile");')
