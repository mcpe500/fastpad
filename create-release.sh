#!/bin/bash
# Script to create GitHub Release with binary attachment
# Usage: ./create-release.sh [GITHUB_TOKEN]

set -e

REPO="mcpe500/fastpad"
TAG="v1.0.0"
BINARY="build/FastPad.exe"
RELEASE_NOTES="RELEASE_NOTES.md"

if [ -z "$1" ]; then
    echo "Error: GitHub token required."
    echo "Usage: $0 YOUR_GITHUB_TOKEN"
    echo ""
    echo "Get a token from: https://github.com/settings/tokens"
    echo "Required scope: repo (Full control of private repositories)"
    exit 1
fi

TOKEN="$1"

echo "📦 Creating GitHub Release for $REPO@$TAG..."
echo ""

# Step 1: Create the release
echo "Step 1: Creating release..."
RELEASE_RESPONSE=$(curl -s -X POST \
    "https://api.github.com/repos/$REPO/releases" \
    -H "Authorization: token $TOKEN" \
    -H "Accept: application/vnd.github.v3+json" \
    -d "{
        \"tag_name\": \"$TAG\",
        \"target_commitish\": \"main\",
        \"name\": \"FastPad v1.0.0 - Initial Release\",
        \"body\": $(cat $RELEASE_NOTES | grep -v '^#' | sed 's/"/\\"/g' | tr '\n' ' ' | head -c 1000),
        \"draft\": false,
        \"prerelease\": false
    }")

RELEASE_ID=$(echo "$RELEASE_RESPONSE" | grep -o '"id": *[0-9]*' | head -1 | grep -o '[0-9]*')

if [ -z "$RELEASE_ID" ]; then
    echo "❌ Failed to create release. Response:"
    echo "$RELEASE_RESPONSE"
    exit 1
fi

echo "✅ Release created (ID: $RELEASE_ID)"
echo ""

# Step 2: Upload the binary
echo "Step 2: Uploading FastPad.exe..."
UPLOAD_RESPONSE=$(curl -s -X POST \
    "https://uploads.github.com/repos/$REPO/releases/$RELEASE_ID/assets?name=FastPad.exe" \
    -H "Authorization: token $TOKEN" \
    -H "Accept: application/vnd.github.v3+json" \
    -H "Content-Type: application/octet-stream" \
    --data-binary @"$BINARY")

echo "✅ Binary uploaded!"
echo ""

# Step 3: Upload release notes
echo "Step 3: Uploading release notes..."
NOTES_RESPONSE=$(curl -s -X PATCH \
    "https://api.github.com/repos/$REPO/releases/$RELEASE_ID" \
    -H "Authorization: token $TOKEN" \
    -H "Accept: application/vnd.github.v3+json" \
    -d "{
        \"body\": $(python3 -c "import json; print(json.dumps(open('$RELEASE_NOTES').read()))" 2>/dev/null || echo '""')
    }")

echo "✅ Release notes updated!"
echo ""

echo "🎉 Release created successfully!"
echo "📎 View at: https://github.com/$REPO/releases/tag/$TAG"
echo ""
echo "Release response:"
echo "$RELEASE_RESPONSE" | head -20
