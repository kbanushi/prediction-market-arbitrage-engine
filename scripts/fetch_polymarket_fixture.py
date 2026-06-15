#!/usr/bin/env python3

import argparse
import json
import pathlib
import ssl
import sys
import urllib.parse
import urllib.request
from typing import Any


GAMMA_EVENTS_URL = "https://gamma-api.polymarket.com/events"
CLOB_BOOK_URL = "https://clob.polymarket.com/book"


def make_ssl_context(insecure: bool) -> ssl.SSLContext:
    if insecure:
        return ssl._create_unverified_context()

    try:
        import certifi  # type: ignore

        return ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        return ssl.create_default_context()


def fetch_json(url: str, ctx: ssl.SSLContext) -> Any:
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "prediction-market-arbitrage-engine-fixture-fetcher",
            "Accept": "application/json",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=20, context=ctx) as response:
            return json.loads(response.read().decode("utf-8"))
    except ssl.SSLCertVerificationError as error:
        print(
            "SSL certificate verification failed.\n\n"
            "Fix option 1:\n"
            "  python3 -m pip install certifi\n\n"
            "Fix option 2, if using python.org Python on macOS:\n"
            "  Open the Python folder in /Applications and run Install Certificates.command\n\n"
            "Temporary debug-only option:\n"
            "  rerun this script with --insecure\n",
            file=sys.stderr,
        )
        raise error


def parse_json_string(value: Any, fallback: Any) -> Any:
    if value is None:
        return fallback

    if isinstance(value, str):
        try:
            return json.loads(value)
        except json.JSONDecodeError:
            return fallback

    return value


def normalize_text(value: Any) -> str:
    return str(value or "").lower()


def event_matches(event: dict[str, Any], query: str | None, event_slug: str | None) -> bool:
    if event_slug:
        return str(event.get("slug") or "").lower() == event_slug.lower()

    if not query:
        return True

    q = query.lower()

    searchable = " ".join(
        [
            normalize_text(event.get("title")),
            normalize_text(event.get("slug")),
            normalize_text(event.get("ticker")),
            normalize_text(event.get("description")),
        ]
    )

    if q in searchable:
        return True

    for market in event.get("markets", []) or []:
        market_text = " ".join(
            [
                normalize_text(market.get("question")),
                normalize_text(market.get("slug")),
                normalize_text(market.get("groupItemTitle")),
            ]
        )

        if q in market_text:
            return True

    return False


def market_is_eligible(market: dict[str, Any]) -> bool:
    return (
        market.get("active") is True
        and market.get("closed") is False
        and market.get("acceptingOrders") is True
        and market.get("enableOrderBook") is True
        and market.get("clobTokenIds") is not None
    )


def get_outcome_token(
    market: dict[str, Any],
    outcome_name: str,
) -> tuple[str, int, list[str], list[str]]:
    outcomes = parse_json_string(market.get("outcomes"), [])
    token_ids = parse_json_string(market.get("clobTokenIds"), [])

    if not isinstance(outcomes, list) or not isinstance(token_ids, list):
        raise ValueError("market outcomes/clobTokenIds were not valid arrays")

    if len(outcomes) != len(token_ids):
        raise ValueError("outcomes and clobTokenIds lengths do not match")

    wanted = outcome_name.lower()

    for idx, outcome in enumerate(outcomes):
        if str(outcome).lower() == wanted:
            return str(token_ids[idx]), idx, [str(x) for x in outcomes], [str(x) for x in token_ids]

    raise ValueError(f"outcome {outcome_name!r} not found in {outcomes}")


def discover_market(
    events: list[dict[str, Any]],
    query: str | None,
    event_slug: str | None,
    outcome: str,
) -> tuple[dict[str, Any], dict[str, Any], str, int, list[str], list[str]]:
    candidates: list[tuple[dict[str, Any], dict[str, Any], str, int, list[str], list[str]]] = []

    for event in events:
        if not event_matches(event, query, event_slug):
            continue

        for market in event.get("markets", []) or []:
            if not market_is_eligible(market):
                continue

            try:
                token_id, outcome_index, outcomes, token_ids = get_outcome_token(market, outcome)
            except ValueError:
                continue

            candidates.append((event, market, token_id, outcome_index, outcomes, token_ids))

    if not candidates:
        raise RuntimeError(
            "No eligible market found. Try a different --query, --event-slug, or --outcome."
        )

    def score(candidate: tuple[dict[str, Any], dict[str, Any], str, int, list[str], list[str]]) -> float:
        _, market, _, _, _, _ = candidate

        for key in ("volume24hrClob", "volume24hr", "liquidityClob", "liquidityNum"):
            value = market.get(key)
            try:
                return float(value)
            except (TypeError, ValueError):
                pass

        return 0.0

    candidates.sort(key=score, reverse=True)
    return candidates[0]


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Fetch a real Polymarket CLOB book fixture."
    )

    parser.add_argument(
        "--token-id",
        help="Optional explicit Polymarket CLOB token_id. If omitted, the script discovers one from Gamma.",
    )

    parser.add_argument(
        "--query",
        help='Optional text search over event title/slug and market question, e.g. "Fed Decision in June".',
    )

    parser.add_argument(
        "--event-slug",
        help="Optional exact Polymarket event slug, e.g. fed-decision-in-june-825.",
    )

    parser.add_argument(
        "--outcome",
        default="Yes",
        help='Outcome token to fetch when discovering from Gamma. Default: "Yes".',
    )

    parser.add_argument(
        "--name",
        default=None,
        help="Fixture name. Default is derived from event/market/outcome.",
    )

    parser.add_argument(
        "--limit",
        type=int,
        default=25,
        help="Number of Gamma events to scan. Default: 25.",
    )

    parser.add_argument(
        "--insecure",
        action="store_true",
        help="Disable TLS verification. Debug only; do not use for normal fixture generation.",
    )

    args = parser.parse_args()

    ctx = make_ssl_context(args.insecure)

    metadata: dict[str, Any] = {}

    token_id = args.token_id

    if token_id:
        if token_id.startswith("<") or token_id.endswith(">"):
            raise ValueError(
                "You passed the placeholder token ID. Use a real token ID or omit --token-id."
            )
    else:
        if args.event_slug:
            gamma_query = urllib.parse.urlencode(
                {
                    "slug": args.event_slug,
                }
            )
        else:
            gamma_query = urllib.parse.urlencode(
                {
                    "active": "true",
                    "closed": "false",
                    "order": "volume_24hr",
                    "ascending": "false",
                    "limit": str(args.limit),
                }
            )

        gamma_url = f"{GAMMA_EVENTS_URL}?{gamma_query}"
        events = fetch_json(gamma_url, ctx)

        if not isinstance(events, list):
            raise RuntimeError("Gamma events response was not a JSON array")

        event, market, token_id, outcome_index, outcomes, token_ids = discover_market(
            events=events,
            query=None if args.event_slug else args.query,
            event_slug=args.event_slug,
            outcome=args.outcome,
        )

        metadata = {
            "event_id": event.get("id"),
            "event_slug": event.get("slug"),
            "event_title": event.get("title"),
            "market_id": market.get("id"),
            "market_question": market.get("question"),
            "condition_id": market.get("conditionId"),
            "outcome": args.outcome,
            "outcome_index": outcome_index,
            "outcomes": outcomes,
            "clob_token_ids": token_ids,
            "selected_token_id": token_id,
            "best_bid_from_gamma": market.get("bestBid"),
            "best_ask_from_gamma": market.get("bestAsk"),
            "accepting_orders": market.get("acceptingOrders"),
            "enable_order_book": market.get("enableOrderBook"),
        }

    book_query = urllib.parse.urlencode({"token_id": token_id})
    book_url = f"{CLOB_BOOK_URL}?{book_query}"

    book = fetch_json(book_url, ctx)

    out_dir = pathlib.Path("tests/fixtures/polymarket")
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.name:
        fixture_name = args.name
    elif metadata:
        slug = str(metadata.get("event_slug") or "polymarket")
        market_id = str(metadata.get("market_id") or "market")
        outcome = str(metadata.get("outcome") or "outcome").lower()
        fixture_name = f"{slug}_{market_id}_{outcome}"
    else:
        fixture_name = f"token_{str(token_id)[:12]}"

    book_path = out_dir / f"book_{fixture_name}.json"
    meta_path = out_dir / f"book_{fixture_name}.metadata.json"

    with book_path.open("w", encoding="utf-8") as f:
        json.dump(book, f, indent=2, sort_keys=True)
        f.write("\n")

    if metadata:
        metadata["book_asset_id"] = book.get("asset_id")
        metadata["book_market"] = book.get("market")
        metadata["book_timestamp"] = book.get("timestamp")
        metadata["book_bid_levels"] = len(book.get("bids", []) or [])
        metadata["book_ask_levels"] = len(book.get("asks", []) or [])

        with meta_path.open("w", encoding="utf-8") as f:
            json.dump(metadata, f, indent=2, sort_keys=True)
            f.write("\n")

    print(f"Wrote {book_path}")

    if metadata:
        print(f"Wrote {meta_path}")
        print(f"event:    {metadata.get('event_title')}")
        print(f"market:   {metadata.get('market_question')}")
        print(f"outcome:  {metadata.get('outcome')}")
        print(f"token_id: {metadata.get('selected_token_id')}")

    print(f"bids:     {len(book.get('bids', []) or [])}")
    print(f"asks:     {len(book.get('asks', []) or [])}")


if __name__ == "__main__":
    main()