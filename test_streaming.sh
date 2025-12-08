#!/bin/bash
# test_streaming.sh
# Test script to simulate streaming commands to the emulator

echo "Testing website streaming functionality..."
echo ""

# Send stream start command
echo "1. Starting stream..."
echo '{"type":"streamStart","url":"https://example.com","quality":"medium"}' | \
    docker exec -i threadxbrowser-emulator-1 /app/build/threadx_browser_sim &
sleep 2

# Send some stream updates
for i in {1..5}; do
    echo "2. Sending frame update $i..."
    echo "{\"type\":\"streamUpdate\",\"frame\":$i}" | \
        docker exec -i threadxbrowser-emulator-1 sh -c 'cat' 2>/dev/null || true
    sleep 1
done

# Send stop command
echo "3. Stopping stream..."
echo '{"type":"streamStop"}' | \
    docker exec -i threadxbrowser-emulator-1 sh -c 'cat' 2>/dev/null || true

echo ""
echo "Test complete! Check the emulator logs:"
echo "  docker-compose logs emulator"
