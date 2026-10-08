#!/usr/bin/env bash
# Configure OpenCR access and build this package on a Raspberry Pi.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./setup_pi.sh --device /dev/ttyACM0 [--ros-distro lyrical]

Connect the OpenCR first. Select its actual ttyACM device; the script reads
its USB vendor/product IDs and creates /dev/opencr from those IDs.
ROS 2, colcon, and the package dependencies must already be installed.
Run this script as the regular Pi user, not with sudo.
EOF
}

device=''
ros_distro='lyrical'
while (($#)); do
  case "$1" in
    --device|--ros-distro)
      option=$1
      if (($# < 2)); then
        printf 'Missing value for %s\n' "$option" >&2
        usage >&2
        exit 2
      fi
      if [[ $option == --device ]]; then
        device=$2
      else
        ros_distro=$2
      fi
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown option: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ! $device =~ ^/dev/ttyACM[0-9]+$ ]] || [[ ! -c $device ]]; then
  printf 'Connect the OpenCR and pass its /dev/ttyACM* device with --device.\n' >&2
  exit 1
fi
if [[ ! $ros_distro =~ ^[a-zA-Z0-9_-]+$ ]]; then
  printf 'Invalid ROS distro name: %s\n' "$ros_distro" >&2
  exit 1
fi
if ((EUID == 0)); then
  printf 'Run as the regular Pi user; the script calls sudo where needed.\n' >&2
  exit 1
fi

for command_name in sudo udevadm getent colcon; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    printf 'Required command is missing: %s\n' "$command_name" >&2
    exit 1
  fi
done
for group_name in dialout video; do
  if ! getent group "$group_name" >/dev/null; then
    printf 'Required group is missing: %s\n' "$group_name" >&2
    exit 1
  fi
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
workspace_dir=$(cd -- "$script_dir/../.." && pwd -P)
ros_setup="/opt/ros/$ros_distro/setup.bash"
if [[ ! -f $ros_setup ]]; then
  printf 'ROS setup file is missing: %s\n' "$ros_setup" >&2
  exit 1
fi

vendor_id=''
product_id=''
while IFS='=' read -r key value; do
  case "$key" in
    ID_VENDOR_ID) vendor_id=$value ;;
    ID_MODEL_ID) product_id=$value ;;
  esac
done < <(udevadm info --query=property --name="$device")
if [[ ! $vendor_id =~ ^[[:xdigit:]]{4}$ ]] || [[ ! $product_id =~ ^[[:xdigit:]]{4}$ ]]; then
  printf 'Could not read valid USB vendor/product IDs from %s.\n' "$device" >&2
  exit 1
fi
vendor_id=${vendor_id,,}
product_id=${product_id,,}

rule_path=/etc/udev/rules.d/99-opencr.rules
printf -v rule_line 'SUBSYSTEM=="tty", ATTRS{idVendor}=="%s", ATTRS{idProduct}=="%s", GROUP="dialout", MODE="0660", SYMLINK+="opencr"' "$vendor_id" "$product_id"
printf 'OpenCR device: %s (vendor %s, product %s)\n' "$device" "$vendor_id" "$product_id"

sudo -v
if sudo test -e "$rule_path"; then
  existing_rule=$(sudo cat "$rule_path")
  if [[ $existing_rule != "$rule_line" ]]; then
    printf 'An existing udev rule differs: %s. Review it before replacing it.\n' "$rule_path" >&2
    exit 1
  fi
fi

target_user=$(id -un)
current_groups=" $(id -nG "$target_user") "
missing_groups=()
for group_name in dialout video; do
  if [[ $current_groups != *" $group_name "* ]]; then
    missing_groups+=("$group_name")
  fi
done
if ((${#missing_groups[@]})); then
  group_list=$(IFS=,; printf '%s' "${missing_groups[*]}")
  sudo usermod -aG "$group_list" "$target_user"
  printf 'Added %s to %s. Log out and back in before using the devices.\n' "$target_user" "$group_list"
fi

if ! sudo test -e "$rule_path"; then
  printf '%s\n' "$rule_line" | sudo tee "$rule_path" >/dev/null
  sudo chmod 0644 "$rule_path"
  printf 'Installed %s\n' "$rule_path"
fi
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=tty
sudo udevadm settle
if [[ -L /dev/opencr ]]; then
  printf 'OpenCR link: %s -> %s\n' /dev/opencr "$(readlink -f /dev/opencr)"
else
  printf 'Reconnect the OpenCR USB cable, then check: ls -l /dev/opencr\n'
fi

# ROS setup files may reference unset shell variables.
set +u
source "$ros_setup"
set -u
if ! command -v ros2 >/dev/null 2>&1; then
  printf 'The ROS setup did not provide the ros2 command: %s\n' "$ros_setup" >&2
  exit 1
fi
cd -- "$workspace_dir"
colcon build --packages-select dynamixel
set +u
source install/setup.bash
set -u
ros2 pkg executables dynamixel
printf 'Setup complete. Log out and back in if group membership changed.\n'
